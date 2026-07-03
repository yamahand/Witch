#pragma once

namespace witch {

/// 基準視界（仮想解像度）。カメラが常にこの範囲を映し、ウィンドウへは
/// 一様スケール + レターボックスで写像される（Engine の IRenderer 参照）。
/// 値は仮置き。アート方針（HD / ドット絵、1 タイルの px 相当）決定時に確定する。
inline constexpr int kDesignWidth  = 1920;
inline constexpr int kDesignHeight = 1080;

} // namespace witch
