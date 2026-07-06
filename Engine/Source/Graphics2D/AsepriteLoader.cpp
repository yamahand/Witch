#include "Graphics2D/AsepriteLoader.h"
#include "WitchEngine/Core/Logger.h"

#include <stb_image.h>  // zlib 展開（stbi_zlib_decode_malloc）だけを借りる

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>

// .ase バイナリ形式のリファレンス:
// https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md
// 対応範囲（pragmatic）:
//   - カラーモード: RGBA / グレースケール / インデックス
//   - セル: raw / zlib 圧縮 / リンク。タイルマップセルは非対応（警告してスキップ）
//   - レイヤー: 可視フラグ・グループの可視継承・不透明度。ブレンドは Normal のみ
//     （他モードは警告して Normal 扱い）
//   - タグ: from/to・ループ方向・repeat

namespace witch::aseprite {

namespace {

// ── バイトリーダー ───────────────────────────────────────────────────────────
// 範囲外読みで ok_ を落として 0 を返す。呼び出し側は節目で ok() を確認する
// （throw しない方針のため、毎回 expected を返す代わりの失敗フラグ方式）。
class Reader {
public:
    explicit Reader(std::span<const uint8_t> data) : data_(data) {}

    bool ok() const { return ok_; }
    size_t pos() const { return pos_; }
    size_t size() const { return data_.size(); }

    void Seek(size_t p) {
        if (p > data_.size()) ok_ = false;
        else pos_ = p;
    }
    void Skip(size_t n) {
        if (n > data_.size() - pos_) ok_ = false;
        else pos_ += n;
    }
    uint8_t U8() {
        if (pos_ >= data_.size()) { ok_ = false; return 0; }
        return data_[pos_++];
    }
    uint16_t U16() {
        const uint16_t lo = U8();
        const uint16_t hi = U8();
        return static_cast<uint16_t>(lo | (hi << 8));
    }
    uint32_t U32() {
        const uint32_t lo = U16();
        const uint32_t hi = U16();
        return lo | (hi << 16);
    }
    int16_t S16() { return static_cast<int16_t>(U16()); }

    std::span<const uint8_t> Bytes(size_t n) {
        if (n > data_.size() - pos_) { ok_ = false; return {}; }
        auto s = data_.subspan(pos_, n);
        pos_ += n;
        return s;
    }
    // .ase の STRING（WORD 長 + UTF-8）。
    std::string String() {
        const size_t len = U16();
        auto bytes = Bytes(len);
        if (bytes.empty()) return {};
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

private:
    std::span<const uint8_t> data_;
    size_t pos_ = 0;
    bool ok_ = true;
};

// ── 8bit 演算ヘルパ ──────────────────────────────────────────────────────────
inline uint8_t Mul8(uint32_t a, uint32_t b) {
    const uint32_t t = a * b + 128;
    return static_cast<uint8_t>((t + (t >> 8)) >> 8);
}

// src over dst（straight alpha 同士）。dst を上書きする。
inline void BlendNormal(uint8_t* dst, const uint8_t* src, uint8_t srcAlphaScale) {
    const uint32_t sa = Mul8(src[3], srcAlphaScale);
    if (sa == 0) return;
    const uint32_t da  = dst[3];
    const uint32_t den = sa * 255 + da * (255 - sa);  // = outAlpha * 255
    if (den == 0) return;
    for (int c = 0; c < 3; ++c) {
        const uint32_t num = src[c] * sa * 255 + dst[c] * da * (255 - sa);
        dst[c] = static_cast<uint8_t>(num / den);
    }
    dst[3] = static_cast<uint8_t>((den + 127) / 255);
}

// ── パース中の内部表現 ───────────────────────────────────────────────────────
struct Layer {
    bool drawable = false;     // 通常レイヤーかつグループ含め可視
    bool background = false;
    uint8_t opacity = 255;
};

struct Cel {
    size_t layerIndex = 0;
    int x = 0, y = 0, w = 0, h = 0;
    uint8_t opacity = 255;
    std::vector<uint8_t> rgba;  // 変換済み RGBA（w*h*4）
};

// サイズ上限。U16 由来の値（最大 65535）をそのまま信用すると、破損した /
// 悪意ある .ase でアトラス確保やセル展開が数百 GB 級になり、vector::assign が
// throw してエンジンの no-throw 方針を破る。上限超過は expected のエラーで返す。
constexpr int kMaxCanvasDim = 8192;    ///< キャンバス・セルの 1 辺の px 上限
constexpr int kMaxAtlasDim  = 16384;   ///< アトラスの 1 辺の px 上限（D3D12 のテクスチャ上限）

constexpr uint16_t kFileMagic  = 0xA5E0;
constexpr uint16_t kFrameMagic = 0xF1FA;

constexpr uint16_t kChunkOldPalette = 0x0004;
constexpr uint16_t kChunkLayer      = 0x2004;
constexpr uint16_t kChunkCel        = 0x2005;
constexpr uint16_t kChunkTags       = 0x2018;
constexpr uint16_t kChunkPalette    = 0x2019;

// ピクセル列（.ase ネイティブ形式）→ RGBA へ変換する。
// @return 失敗時 false（インデックスがパレット範囲外等は透明として許容し true）
bool ConvertPixels(std::span<const uint8_t> raw, int w, int h, int colorDepth,
                   const std::vector<std::array<uint8_t, 4>>& palette,
                   int transparentIndex, bool background,
                   std::vector<uint8_t>& outRgba) {
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    outRgba.assign(count * 4, 0);
    switch (colorDepth) {
    case 32:
        if (raw.size() < count * 4) return false;
        std::memcpy(outRgba.data(), raw.data(), count * 4);
        return true;
    case 16:  // グレースケール: value, alpha
        if (raw.size() < count * 2) return false;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t v = raw[i * 2 + 0];
            outRgba[i * 4 + 0] = v;
            outRgba[i * 4 + 1] = v;
            outRgba[i * 4 + 2] = v;
            outRgba[i * 4 + 3] = raw[i * 2 + 1];
        }
        return true;
    case 8:  // インデックス
        if (raw.size() < count) return false;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t idx = raw[i];
            // 背景レイヤー以外では透明色インデックスを透明として扱う。
            if (!background && static_cast<int>(idx) == transparentIndex) continue;
            if (static_cast<size_t>(idx) >= palette.size()) continue;
            const auto& c = palette[idx];
            outRgba[i * 4 + 0] = c[0];
            outRgba[i * 4 + 1] = c[1];
            outRgba[i * 4 + 2] = c[2];
            outRgba[i * 4 + 3] = c[3];
        }
        return true;
    default:
        return false;
    }
}

} // namespace

std::expected<ParseResult, std::string> ParseAse(std::span<const uint8_t> bytes,
                                                 std::string_view sourceName) {
    Reader r(bytes);

    // ── ヘッダ（128 バイト）─────────────────────────────────────────────────
    r.U32();  // file size（信用せず実バッファサイズで検証する）
    if (r.U16() != kFileMagic)
        return std::unexpected(std::format("{}: not an Aseprite file (bad magic)", sourceName));
    const int frameCount = r.U16();
    const int canvasW    = r.U16();
    const int canvasH    = r.U16();
    const int colorDepth = r.U16();
    const uint32_t headerFlags = r.U32();
    const bool layerOpacityValid = (headerFlags & 1u) != 0;
    r.U16();               // speed（deprecated）
    r.U32(); r.U32();      // 0, 0
    const int transparentIndex = r.U8();
    r.Skip(3);             // ignore
    r.U16();               // number of colors
    r.Seek(128);           // 残り（pixel ratio / grid / reserved）は読み飛ばす

    if (!r.ok() || frameCount <= 0 || canvasW <= 0 || canvasH <= 0)
        return std::unexpected(std::format("{}: broken header", sourceName));
    if (canvasW > kMaxCanvasDim || canvasH > kMaxCanvasDim)
        return std::unexpected(std::format(
            "{}: canvas too large ({}x{}, limit {})", sourceName, canvasW, canvasH, kMaxCanvasDim));
    if (colorDepth != 32 && colorDepth != 16 && colorDepth != 8)
        return std::unexpected(std::format("{}: unsupported color depth {}", sourceName, colorDepth));

    const int bytesPerPixel = colorDepth / 8;

    ParseResult result;
    result.frameWidth  = canvasW;
    result.frameHeight = canvasH;

    std::vector<Layer> layers;
    // グループの可視継承: visibleAtLevel[L] = childLevel L のレイヤー/グループの実効可視。
    std::vector<bool> visibleAtLevel;
    std::vector<std::array<uint8_t, 4>> palette;
    std::vector<std::vector<Cel>> frameCels(static_cast<size_t>(frameCount));
    std::vector<float> durations(static_cast<size_t>(frameCount), 0.1f);
    bool warnedBlend = false;
    bool warnedTilemap = false;

    // ── フレーム列 ──────────────────────────────────────────────────────────
    for (int frame = 0; frame < frameCount; ++frame) {
        const size_t frameStart = r.pos();
        const uint32_t frameBytes = r.U32();
        if (r.U16() != kFrameMagic)
            return std::unexpected(std::format("{}: broken frame {} (bad magic)", sourceName, frame));
        const uint16_t oldChunks = r.U16();
        const uint16_t durationMs = r.U16();
        r.Skip(2);
        const uint32_t newChunks = r.U32();
        const uint32_t chunkCount = newChunks != 0 ? newChunks : oldChunks;

        durations[static_cast<size_t>(frame)] =
            static_cast<float>(durationMs != 0 ? durationMs : 100) / 1000.0f;

        for (uint32_t i = 0; i < chunkCount && r.ok(); ++i) {
            const size_t chunkStart = r.pos();
            const uint32_t chunkSize = r.U32();
            const uint16_t chunkType = r.U16();
            if (chunkSize < 6) return std::unexpected(std::format("{}: broken chunk", sourceName));
            const size_t chunkEnd = chunkStart + chunkSize;

            switch (chunkType) {
            case kChunkLayer: {
                const uint16_t flags = r.U16();
                const uint16_t type = r.U16();
                const uint16_t childLevel = r.U16();
                r.U16(); r.U16();  // default width/height（未使用）
                const uint16_t blendMode = r.U16();
                const uint8_t opacity = r.U8();
                r.Skip(3);
                r.String();  // name（診断で欲しくなるまで捨てる）

                const size_t level = childLevel;
                const bool ownVisible = (flags & 1u) != 0;
                const bool parentVisible = level == 0 || (level <= visibleAtLevel.size()
                                            && visibleAtLevel[level - 1]);
                const bool visible = ownVisible && parentVisible;
                if (visibleAtLevel.size() <= level)
                    visibleAtLevel.resize(level + 1, false);
                visibleAtLevel[level] = visible;

                Layer layer;
                layer.drawable   = visible && type == 0;  // 通常レイヤーのみ描画対象
                layer.background = (flags & 8u) != 0;
                layer.opacity    = layerOpacityValid ? opacity : 255;
                if (type == 2 && !warnedTilemap) {
                    log::Warn("Aseprite {}: tilemap layer is not supported; skipped", sourceName);
                    warnedTilemap = true;
                }
                if (layer.drawable && blendMode != 0 && !warnedBlend) {
                    log::Warn("Aseprite {}: blend mode {} is not supported; treated as Normal",
                              sourceName, blendMode);
                    warnedBlend = true;
                }
                layers.push_back(layer);
                break;
            }
            case kChunkCel: {
                Cel cel;
                cel.layerIndex = r.U16();
                cel.x = r.S16();
                cel.y = r.S16();
                cel.opacity = r.U8();
                const uint16_t celType = r.U16();
                r.S16();   // z-index（未対応。通常 0）
                r.Skip(5);

                if (cel.layerIndex >= layers.size()) break;  // 壊れたファイル: 黙って捨てない方が良いが描画は継続
                const Layer& layer = layers[cel.layerIndex];

                if (celType == 1) {  // リンクセル: 過去フレームの同レイヤーのセルを再利用
                    const size_t linkFrame = r.U16();
                    if (linkFrame < frameCels.size()) {
                        for (const Cel& src : frameCels[linkFrame]) {
                            if (src.layerIndex == cel.layerIndex) {
                                Cel copy = src;
                                copy.opacity = cel.opacity;
                                frameCels[static_cast<size_t>(frame)].push_back(std::move(copy));
                                break;
                            }
                        }
                    }
                    break;
                }
                if (celType == 3) {  // タイルマップセル
                    if (!warnedTilemap) {
                        log::Warn("Aseprite {}: tilemap cel is not supported; skipped", sourceName);
                        warnedTilemap = true;
                    }
                    break;
                }
                if (celType != 0 && celType != 2) break;

                cel.w = r.U16();
                cel.h = r.U16();
                if (cel.w <= 0 || cel.h <= 0) break;
                // セルはキャンバス外へはみ出せるが、キャンバス上限を大きく超える寸法は
                // 破損 / 悪意ある入力とみなす（pixelBytes ≤ 8192^2*4 = 256MB に抑え、
                // 後続の int キャストも安全にする）。
                if (cel.w > kMaxCanvasDim || cel.h > kMaxCanvasDim)
                    return std::unexpected(std::format(
                        "{}: cel too large ({}x{}, limit {}) in frame {}",
                        sourceName, cel.w, cel.h, kMaxCanvasDim, frame));
                const size_t pixelBytes = static_cast<size_t>(cel.w) * static_cast<size_t>(cel.h)
                                        * static_cast<size_t>(bytesPerPixel);

                std::vector<uint8_t> nativePixels;
                if (celType == 0) {  // raw
                    auto raw = r.Bytes(pixelBytes);
                    if (!r.ok()) return std::unexpected(std::format("{}: broken raw cel", sourceName));
                    nativePixels.assign(raw.begin(), raw.end());
                } else {  // zlib 圧縮
                    if (chunkEnd < r.pos() || chunkEnd > r.size())
                        return std::unexpected(std::format("{}: broken compressed cel", sourceName));
                    const size_t compressedSize = chunkEnd - r.pos();
                    auto compressed = r.Bytes(compressedSize);
                    // 固定長の出力バッファへ展開する。malloc 版と違い展開後サイズが
                    // 事前に上限指定でき、細工された zlib ストリーム（decompression
                    // bomb）でもピクセル所要量を超えた時点でエラーになる。
                    nativePixels.resize(pixelBytes);
                    const int decoded = stbi_zlib_decode_buffer(
                        reinterpret_cast<char*>(nativePixels.data()),
                        static_cast<int>(pixelBytes),
                        reinterpret_cast<const char*>(compressed.data()),
                        static_cast<int>(compressedSize));
                    if (decoded < 0 || static_cast<size_t>(decoded) != pixelBytes)
                        return std::unexpected(std::format(
                            "{}: zlib decode failed in frame {} (expected {} bytes, got {})",
                            sourceName, frame, pixelBytes, decoded));
                }

                if (!ConvertPixels(nativePixels, cel.w, cel.h, colorDepth, palette,
                                   transparentIndex, layer.background, cel.rgba))
                    return std::unexpected(std::format("{}: broken cel pixels", sourceName));
                frameCels[static_cast<size_t>(frame)].push_back(std::move(cel));
                break;
            }
            case kChunkTags: {
                const uint16_t tagCount = r.U16();
                r.Skip(8);
                for (uint16_t t = 0; t < tagCount && r.ok(); ++t) {
                    AsepriteTag tag;
                    tag.from = r.U16();
                    tag.to   = r.U16();
                    const uint8_t dir = r.U8();
                    tag.direction = dir <= 3 ? static_cast<AsepriteLoopDir>(dir)
                                             : AsepriteLoopDir::Forward;
                    tag.repeat = r.U16();
                    r.Skip(6 + 3 + 1);  // reserved + RGB + extra
                    tag.name = r.String();
                    tag.from = std::clamp(tag.from, 0, frameCount - 1);
                    tag.to   = std::clamp(tag.to, tag.from, frameCount - 1);
                    result.tags.push_back(std::move(tag));
                }
                break;
            }
            case kChunkPalette: {
                const uint32_t newSize = r.U32();
                const uint32_t first = r.U32();
                const uint32_t last  = r.U32();
                r.Skip(8);
                if (newSize > 4096 || first > last || last >= 4096)
                    return std::unexpected(std::format("{}: broken palette", sourceName));
                if (palette.size() < newSize)
                    palette.resize(newSize, {0, 0, 0, 255});
                for (uint32_t p = first; p <= last && r.ok(); ++p) {
                    const uint16_t entryFlags = r.U16();
                    std::array<uint8_t, 4> color{r.U8(), r.U8(), r.U8(), r.U8()};
                    if (entryFlags & 1u) r.String();  // color name
                    if (p < palette.size()) palette[p] = color;
                }
                break;
            }
            case kChunkOldPalette: {
                // 新パレットチャンク（0x2019）が同居するファイルでは後勝ちで問題ない
                // （値は同じ）。古いファイルへの後方互換として最小限に読む。
                const uint16_t packets = r.U16();
                size_t index = 0;
                for (uint16_t p = 0; p < packets && r.ok(); ++p) {
                    index += r.U8();
                    const size_t count = [&] { const uint8_t c = r.U8(); return c == 0 ? 256u : static_cast<size_t>(c); }();
                    // インデックスは U8（最大 256 色）。kChunkPalette と同じ 4096 上限で、
                    // 破損 / 悪意ある packets 連続による palette の巨大確保（throw）を防ぐ。
                    if (index + count > 4096)
                        return std::unexpected(std::format("{}: broken old palette", sourceName));
                    if (palette.size() < index + count)
                        palette.resize(index + count, {0, 0, 0, 255});
                    for (size_t c = 0; c < count && r.ok(); ++c, ++index)
                        palette[index] = {r.U8(), r.U8(), r.U8(), 255};
                }
                break;
            }
            default:
                break;  // 未対応チャンクは読み飛ばす
            }

            r.Seek(chunkEnd);  // チャンク内で読み残しがあっても次チャンク先頭へ揃える
        }

        r.Seek(frameStart + frameBytes);
        if (!r.ok())
            return std::unexpected(std::format("{}: unexpected end of file in frame {}", sourceName, frame));
    }

    // ── フレーム合成 → アトラス化 ───────────────────────────────────────────
    const int columns = static_cast<int>(
        std::ceil(std::sqrt(static_cast<double>(frameCount))));
    const int rows = (frameCount + columns - 1) / columns;
    result.atlasWidth  = columns * canvasW;
    result.atlasHeight = rows * canvasH;
    // GPU テクスチャにできない巨大アトラスは確保前に弾く（bad_alloc を出さない）。
    if (result.atlasWidth > kMaxAtlasDim || result.atlasHeight > kMaxAtlasDim)
        return std::unexpected(std::format(
            "{}: atlas too large ({}x{} for {} frames of {}x{}, limit {})",
            sourceName, result.atlasWidth, result.atlasHeight, frameCount,
            canvasW, canvasH, kMaxAtlasDim));
    result.atlasPixels.assign(
        static_cast<size_t>(result.atlasWidth) * static_cast<size_t>(result.atlasHeight) * 4, 0);
    result.frames.resize(static_cast<size_t>(frameCount));

    std::vector<uint8_t> canvas;
    for (int frame = 0; frame < frameCount; ++frame) {
        canvas.assign(static_cast<size_t>(canvasW) * static_cast<size_t>(canvasH) * 4, 0);

        // セルをレイヤー順（下 → 上）に合成する。チャンク出現順はレイヤー順と
        // 一致する保証がないため明示的に安定ソートする。
        auto& cels = frameCels[static_cast<size_t>(frame)];
        std::stable_sort(cels.begin(), cels.end(),
                         [](const Cel& a, const Cel& b) { return a.layerIndex < b.layerIndex; });
        for (const Cel& cel : cels) {
            const Layer& layer = layers[cel.layerIndex];
            if (!layer.drawable) continue;
            const uint8_t alphaScale = Mul8(cel.opacity, layer.opacity);
            for (int sy = 0; sy < cel.h; ++sy) {
                const int dy = cel.y + sy;
                if (dy < 0 || dy >= canvasH) continue;
                for (int sx = 0; sx < cel.w; ++sx) {
                    const int dx = cel.x + sx;
                    if (dx < 0 || dx >= canvasW) continue;
                    const size_t si = (static_cast<size_t>(sy) * static_cast<size_t>(cel.w)
                                     + static_cast<size_t>(sx)) * 4;
                    const size_t di = (static_cast<size_t>(dy) * static_cast<size_t>(canvasW)
                                     + static_cast<size_t>(dx)) * 4;
                    BlendNormal(&canvas[di], &cel.rgba[si], alphaScale);
                }
            }
        }

        // 合成済みキャンバスをアトラスのセルへコピーする。
        const int cellX = (frame % columns) * canvasW;
        const int cellY = (frame / columns) * canvasH;
        for (int y = 0; y < canvasH; ++y) {
            const size_t src = static_cast<size_t>(y) * static_cast<size_t>(canvasW) * 4;
            const size_t dst = ((static_cast<size_t>(cellY) + static_cast<size_t>(y))
                                 * static_cast<size_t>(result.atlasWidth)
                               + static_cast<size_t>(cellX)) * 4;
            std::memcpy(&result.atlasPixels[dst], &canvas[src],
                        static_cast<size_t>(canvasW) * 4);
        }

        result.frames[static_cast<size_t>(frame)] = AsepriteFrame{
            .x = cellX, .y = cellY, .duration = durations[static_cast<size_t>(frame)]};
    }

    return result;
}

} // namespace witch::aseprite
