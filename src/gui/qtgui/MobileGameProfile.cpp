// Per game settings of the mobile interface: the same game profiles (gameProfiles/<title id>.ini) as the desktop interface.
#include "MobileBackend.h"

#include "Cafe/GameProfile/GameProfile.h"
#include "Cafe/TitleList/TitleId.h"
#include "config/ActiveSettings.h"

// friend of GameProfile: the profile members are private
class MobileGameProfile
{
  public:
	static void Load(GameProfile& profile, uint64_t titleId)
	{
		profile.Reset();
		profile.Load(titleId);
	}

	static int Get(const GameProfile& p, const std::string& key)
	{
		if (key == "cpu_mode")
		{
			switch (p.m_cpuMode.value_or(CPUMode::Auto))
			{
			case CPUMode::SinglecoreInterpreter:
				return 0;
			case CPUMode::SinglecoreRecompiler:
				return 1;
			case CPUMode::DualcoreRecompiler:
			case CPUMode::MulticoreRecompiler:
				return 2;
			default:
				return 3;
			}
		}
		if (key == "thread_quantum")
			return (int)p.m_threadQuantum;
		if (key == "accurate_shader_mul")
			return p.m_accurateShaderMul == AccurateShaderMulOption::True ? 1 : 0;
		if (key == "precompiled_shaders")
			return (int)p.m_precompiledShaders.value_or(PrecompiledShaderOption::Auto);
		if (key == "load_shared_libraries")
			return p.m_loadSharedLibraries.value_or(true) ? 1 : 0;
		return 0;
	}

	static bool Set(GameProfile& p, const std::string& key, int value)
	{
		if (key == "cpu_mode")
		{
			static const CPUMode modes[] = {CPUMode::SinglecoreInterpreter, CPUMode::SinglecoreRecompiler, CPUMode::MulticoreRecompiler, CPUMode::Auto};
			p.m_cpuMode = modes[std::clamp(value, 0, 3)];
		}
		else if (key == "thread_quantum")
			p.m_threadQuantum = (uint32)std::clamp(value, 5000, 536870912);
		else if (key == "accurate_shader_mul")
			p.m_accurateShaderMul = value ? AccurateShaderMulOption::True : AccurateShaderMulOption::False;
		else if (key == "precompiled_shaders")
			p.m_precompiledShaders = (PrecompiledShaderOption)std::clamp(value, 0, 2);
		else if (key == "load_shared_libraries")
			p.m_loadSharedLibraries = value != 0;
		else
			return false;
		return true;
	}

	static void Save(GameProfile& p, uint64_t titleId)
	{
		p.Save(titleId);
	}
};

namespace MobileBackend
{
	int GetGameProfileSetting(uint64_t titleId, const std::string& key)
	{
		GameProfile profile;
		MobileGameProfile::Load(profile, TitleIdParser::MakeBaseTitleId(titleId));
		return MobileGameProfile::Get(profile, key);
	}

	void SetGameProfileSetting(uint64_t titleId, const std::string& key, int value)
	{
		titleId = TitleIdParser::MakeBaseTitleId(titleId);
		GameProfile profile;
		MobileGameProfile::Load(profile, titleId);
		if (MobileGameProfile::Set(profile, key, value))
			MobileGameProfile::Save(profile, titleId);
	}

	void ResetGameProfile(uint64_t titleId)
	{
		titleId = TitleIdParser::MakeBaseTitleId(titleId);
		std::error_code ec;
		fs::remove(ActiveSettings::GetConfigPath("gameProfiles/{:016x}.ini", titleId), ec);
	}
} // namespace MobileBackend
