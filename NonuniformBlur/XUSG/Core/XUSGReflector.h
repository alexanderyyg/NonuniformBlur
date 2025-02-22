//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGType.h"

namespace XUSG
{
	class Reflector
	{
	public:
		Reflector();
		virtual ~Reflector();

		bool SetShader(const Blob &shader);
		bool IsValid() const;
		uint32_t GetResourceBindingPointByName(const char *name, uint32_t defaultVal = UINT32_MAX) const;

	protected:
		Shader::Reflection m_reflection;
	};
}
