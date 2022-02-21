#pragma once
/*
    �������� Api - https://core.telegram.org/bots/api
    ����� = token
    ������������� ���� - id
    ����� - UTF-8

    ��������� ��������� https://api.telegram.org/bot{token}/sendMessage?chat_id={chatid}text=test
    ��������� ���������� https://api.telegram.org/bot{token}/getUpdates
*/

#ifdef CPPDLL_EXPORTS
    #define DLLIMPORT_EXPORT __declspec(dllexport)
#else
    #define DLLIMPORT_EXPORT __declspec(dllimport)
#endif

#include <combaseapi.h>
#include <memory>
#include <string>
#include <functional>
#include <list>
#include <unordered_map>

#include <tgbot/tgbot.h>

// ����������� ������ � UTF-8
inline DLLIMPORT_EXPORT std::string getUtf8Str(const std::wstring& str);

// ����������� ������ UTF-8 � std::wstring
inline DLLIMPORT_EXPORT std::wstring getUNICODEString(const std::string& utf8Str);

//----------------------------------------------------------------------------//
// ��������� ������������ ��� ��������� ���������� �� �������� ����
interface DLLIMPORT_EXPORT ITelegramAlerter
{
    virtual ~ITelegramAlerter() = default;
    // ���������� � ��������� ������ � ������ �������� ����
    virtual void onAlertFromTelegram(const std::wstring& alertMessage) = 0;
};

typedef std::function<void(const std::wstring&)> TelegramErrorHandler;
typedef TgBot::Message::Ptr MessagePtr;
typedef TgBot::EventBroadcaster::MessageListener CommandFunction;

//----------------------------------------------------------------------------//
interface DLLIMPORT_EXPORT ITelegramThread
{
    // �������� ����������� ������ ����� �������� ��������������� ����� ��-�� ������������
    // longpoll ���������, TODO ���������� �� ���� BoostHttpOnlySslClient
    virtual ~ITelegramThread() = default;

    // ������ ������
    virtual void startTelegramThread(const std::unordered_map<std::string, CommandFunction>& commandsList,
                                     const CommandFunction& onUnknownCommand = nullptr,
                                     const CommandFunction& onNonCommandMessage = nullptr) = 0;

    // ��������� ������
    virtual void stopTelegramThread() = 0;

    // ������� �������� ��������� � ����
    virtual void sendMessage(const std::list<int64_t>& chatIds, const std::wstring& msg,
                             bool disableWebPagePreview = false, int32_t replyToMessageId = 0,
                             TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                             const std::string& parseMode = "", bool disableNotification = false) = 0;

    // ������� �������� ��������� � ���
    virtual void sendMessage(int64_t chatId, const std::wstring& msg, bool disableWebPagePreview = false,
                             int32_t replyToMessageId = 0,
                             TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                             const std::string& parseMode = "", bool disableNotification = false) = 0;

    // ���������� ������� ���� ����� ������ ��� ������������
    virtual TgBot::EventBroadcaster& getBotEvents() = 0;

    // ��������� ��� ����
    virtual const TgBot::Api& getBotApi() = 0;
};
typedef std::unique_ptr<ITelegramThread> ITelegramThreadPtr;

// ������� ��������� ������ ������
inline DLLIMPORT_EXPORT
ITelegramThreadPtr CreateTelegramThread(const std::string& botToken,
                                        ITelegramAlerter* alertInterface = nullptr);

// ������� ��������� ������ ������
inline DLLIMPORT_EXPORT
ITelegramThreadPtr CreateTelegramThread(const std::string& botToken,
                                        const TelegramErrorHandler& errorHandler = nullptr);

// ������� ��������� �������� ����
inline DLLIMPORT_EXPORT
std::unique_ptr<TgBot::Bot> CreateTelegramBot(const std::string& botToken,
                                              const TgBot::HttpClient& client);

// ���������� ������� ���������� �� �������� ������
inline DLLIMPORT_EXPORT
void HandleTgUpdate(const TgBot::EventHandler& handler, TgBot::Update::Ptr update);
