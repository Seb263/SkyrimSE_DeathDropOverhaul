#pragma once

#include "SettingsIni.hpp"

#include "Utils/NiUtils.hpp"

class ModUtils
{

public:

	static void ProcessGridCells(const std::function<void(RE::TESObjectCELL*)>& processCell)
	{
		auto* tes = RE::TES::GetSingleton();
		if (!tes) return;

		if (auto* cell = tes->interiorCell; cell && cell->IsAttached()) {
			processCell(cell);
		} else {
			if (const auto gridLength = tes->gridCells ? tes->gridCells->length : 0; gridLength > 0) {
				for (std::uint32_t x = 0; x < gridLength; ++x) {
					for (std::uint32_t y = 0; y < gridLength; ++y) {
						auto* cell = tes->gridCells->GetCell(x, y);
						if (!cell || !cell->IsAttached()) continue;

						processCell(cell);
					}
				}
			}
		}
	}

	template <class T>
	static void SetObjectCollision(RE::TESObjectREFR* ref, T colLayer)
	{
		if (!ref) return;

		RE::NiAVObject* targetRoot = ref->Get3D();
		if (!targetRoot) return;

		const uint32_t colLayerVal = static_cast<uint32_t>(colLayer);

		std::function<void(RE::NiAVObject*)> findChildNodes = [&](RE::NiAVObject* targetNode) {
			if (!targetNode || !targetNode->AsNode()) return;
			auto* niNode = targetNode->AsNode();

			NiUtils::SetObjectCollision(targetNode, colLayerVal);

			for (auto& child : niNode->GetChildren()) {
				if (child && child.get()) {
					findChildNodes(child.get());
				}
			}
		};

		findChildNodes(targetRoot);
	}

	static void ClampVector4(RE::hkVector4& vec, const float maxMagnitude)
	{
		const float x = vec.quad.m128_f32[0];
		const float y = vec.quad.m128_f32[1];
		const float z = vec.quad.m128_f32[2];

		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
			vec.quad = _mm_setzero_ps();
			return;
		}

		const float magnitudeSq = x * x + y * y + z * z;
		alignas(16) float mask[4] = { 1.0f, 1.0f, 1.0f, 0.0f };

		if (!std::isfinite(magnitudeSq)) {
			vec.quad = _mm_setzero_ps();
			return;
		}

		if (magnitudeSq > maxMagnitude * maxMagnitude) {
			const float scale = maxMagnitude / std::sqrt(magnitudeSq);
			mask[0] = mask[1] = mask[2] = scale;
		}
		vec.quad = _mm_mul_ps(vec.quad, _mm_load_ps(mask));
	}
};
