// Accounts part of the bridge (ported from the Android port, NativeAccount.cpp, without the JNI layer)
#include "MobileBackend.h"

#include "Cafe/Account/Account.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "config/CemuConfig.h"
#include "config/NetworkSettings.h"

#include <boost/nowide/convert.hpp>

#include <chrono>

namespace MobileBackend
{
	namespace
	{
		AccountEntry ToEntry(const Account& account)
		{
			AccountEntry entry;
			entry.persistentId = account.GetPersistentId();
			entry.miiName = boost::nowide::narrow(std::wstring(account.GetMiiName()));
			entry.birthYear = account.GetBirthYear();
			entry.birthMonth = account.GetBirthMonth();
			entry.birthDay = account.GetBirthDay();
			entry.gender = account.GetGender();
			entry.email = std::string(account.GetEmail());
			entry.country = static_cast<int>(account.GetCountry());
			entry.validOnline = account.IsValidOnlineAccount();
			return entry;
		}

		bool AccountExists(uint32_t persistentId)
		{
			for (const auto& account : Account::GetAccounts())
			{
				if (account.GetPersistentId() == persistentId)
					return true;
			}
			return false;
		}
	} // namespace

	std::vector<AccountEntry> GetAccounts()
	{
		std::vector<AccountEntry> result;
		for (const auto& account : Account::GetAccounts())
			result.push_back(ToEntry(account));
		return result;
	}

	std::vector<AccountCountry> GetAccountCountries()
	{
		std::vector<AccountCountry> result;
		for (int i = 0; i < static_cast<int>(NCrypto::GetCountryCount()); ++i)
		{
			const char* name = NCrypto::GetCountryAsString(i);
			// "NN" means unknown, only index 0 (= not set) is kept
			if (name && (i == 0 || std::string_view(name) != "NN"))
				result.push_back({i, name});
		}
		return result;
	}

	bool CreateAccount(uint32_t persistentId, const std::string& miiName, std::string& errorOut)
	{
		if (persistentId < kMinPersistentId)
		{
			errorOut = "invalid persistent id";
			return false;
		}
		if (AccountExists(persistentId))
		{
			errorOut = "persistent id already used";
			return false;
		}
		if (miiName.empty())
		{
			errorOut = "empty name";
			return false;
		}
		if (Account::GetAccounts().size() >= static_cast<size_t>(kMaxAccountCount))
		{
			errorOut = "too many accounts";
			return false;
		}
		Account account(persistentId, boost::nowide::widen(miiName));
		if (account.Save())
		{
			errorOut = "can not write the account file";
			return false;
		}
		Account::RefreshAccounts();
		return true;
	}

	bool SaveAccount(const AccountEntry& entry)
	{
		if (!AccountExists(entry.persistentId))
			return false;
		Account account = Account::GetAccount(entry.persistentId); // copy
		account.SetMiiName(boost::nowide::widen(entry.miiName.empty() ? std::string("default") : entry.miiName));
		account.SetCountry(static_cast<uint32>(entry.country));
		account.SetGender(static_cast<uint8>(entry.gender));
		account.SetEmail(entry.email);
		const std::chrono::year_month_day birthday(std::chrono::year(entry.birthYear),
													std::chrono::month(entry.birthMonth),
													std::chrono::day(entry.birthDay));
		if (birthday.ok())
		{
			account.SetBirthYear(static_cast<uint16>(entry.birthYear));
			account.SetBirthMonth(static_cast<uint8>(entry.birthMonth));
			account.SetBirthDay(static_cast<uint8>(entry.birthDay));
		}
		const bool failed = static_cast<bool>(account.Save());
		Account::RefreshAccounts();
		return !failed;
	}

	bool DeleteAccount(uint32_t persistentId)
	{
		if (Account::GetAccounts().size() <= 1 || !AccountExists(persistentId))
			return false;
		const fs::path path = Account::GetAccount(persistentId).GetFileName();
		try
		{
			fs::remove_all(path.parent_path());
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "Failed to delete account {}: {}", path.string(), ex.what());
			return false;
		}
		Account::RefreshAccounts();
		return true;
	}

	uint32_t GetActiveAccountId()
	{
		// falls back to the first account like the desktop interface does when the saved one is gone
		const uint32_t id = GetConfig().account.m_persistent_id;
		if (AccountExists(id))
			return id;
		const auto& accounts = Account::GetAccounts();
		return accounts.empty() ? id : accounts.front().GetPersistentId();
	}

	void SetActiveAccountId(uint32_t persistentId)
	{
		if (!AccountExists(persistentId))
			return;
		GetConfig().account.m_persistent_id = persistentId;
		GetConfigHandle().Save();
	}

	int GetAccountNetworkService(uint32_t persistentId)
	{
		return static_cast<int>(GetConfig().GetAccountNetworkService(persistentId));
	}

	void SetAccountNetworkService(uint32_t persistentId, int service)
	{
		if (service < 0 || service > static_cast<int>(NetworkService::COUNT))
			return;
		GetConfig().SetAccountSelectedService(persistentId, static_cast<NetworkService>(service));
		GetConfigHandle().Save();
	}

	bool IsOTPPresent()
	{
		return NCrypto::OTP_IsPresent();
	}

	bool IsSEEPROMPresent()
	{
		return NCrypto::SEEPROM_IsPresent();
	}

	std::vector<std::string> GetAccountValidationErrors(uint32_t persistentId)
	{
		std::vector<std::string> errors;
		if (!AccountExists(persistentId))
			return errors;
		const Account& account = Account::GetAccount(persistentId);
		const OnlineValidator validator = account.ValidateOnlineFiles();
		if (validator.IsValid())
			return errors;

		if (validator.otp == OnlineValidator::FileState::Missing)
			errors.emplace_back("missingOTP");
		else if (validator.otp == OnlineValidator::FileState::Corrupted)
			errors.emplace_back("corruptedOTP");

		if (validator.seeprom == OnlineValidator::FileState::Missing)
			errors.emplace_back("missingSEEPROM");
		else if (validator.seeprom == OnlineValidator::FileState::Corrupted)
			errors.emplace_back("corruptedSEEPROM");

		int shown = 0;
		for (const auto& missing : validator.missing_files)
		{
			if (++shown > 10)
				break;
			errors.push_back("missingFile:" + boost::nowide::narrow(missing));
		}

		if (!validator.valid_account && validator.account_error != OnlineAccountError::kNone)
			errors.push_back("accountError:" + std::to_string(static_cast<int>(validator.account_error)));
		return errors;
	}
} // namespace MobileBackend
