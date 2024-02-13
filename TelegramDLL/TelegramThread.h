#pragma once
/*
   Api description - https://core.telegram.org/bots/api
   token = token
   Chat ID - id
   Text - UTF-8

   Send message https://api.telegram.org/bot{token}/SendMessage?chat_id={chatid}text=test
   Check for updates https://api.telegram.org/bot{token}/getUpdates
*/

#ifdef CPPDLL_EXPORTS
    #define DLLIMPORT_EXPORT __declspec(dllexport)
#else
    #define DLLIMPORT_EXPORT __declspec(dllimport)
#endif

#include <memory>
#include <string>
#include <functional>
#include <list>

#pragma warning( push )
#pragma warning( disable: 4996 ) // boost deprecated objects usage
#include <tgbot/tgbot.h>
#pragma warning( pop )

// convert the string to UTF-8
inline DLLIMPORT_EXPORT std::string getUtf8Str(const std::wstring& str);

// convert UTF-8 string to std::wstring
inline DLLIMPORT_EXPORT std::wstring getUNICODEString(const std::string& utf8Str);

//------------------------------------------------ ----------------------//
// interface used to receive notifications from the telegram bot
struct DLLIMPORT_EXPORT ITelegramAlerter
{
    virtual ~ITelegramAlerter() = default;
    // notification of an error in the work of the telegram bot
    virtual void onAlertFromTelegram(const std::wstring& alertMessage) = 0;
};

typedef std::function<void(const std::wstring&)> TelegramErrorHandler;
typedef TgBot::Message::Ptr MessagePtr;
typedef TgBot::EventBroadcaster::MessageListener CommandCallback;

//----------------------------------------------------------------------------//
struct DLLIMPORT_EXPORT ITelegramThread
{
    // deleting a running thread can take a long time due to
    // longpoll requests, TODO to convert to your own BoostHttpOnlySslClient
    virtual ~ITelegramThread() = default;

    // https://core.telegram.org/bots/api#botcommand
    struct CommandInfo
    {
        // Text of the command; 1-32 characters. Can contain only lowercase English letters, digits and underscores.
        std::wstring command;
        // Description of the command; 1-256 characters.
        std::wstring description;
        // Callback for the command
        CommandCallback callback;
    };
    // start thread and set callbacks
    // will throw exception on API call error
    virtual void StartTelegramThread(const std::list<CommandInfo>& commandsList,
                                     const CommandCallback& onUnknownCommand = nullptr,
                                     const CommandCallback& OnNonCommandMessage = nullptr) = 0;

    // stop thread
    virtual void StopTelegramThread() = 0;

    // Get current bot commands
    virtual std::list<std::pair<std::wstring, std::wstring>> GetCommands() const = 0;

    // send message to chats
    virtual void SendMessage(const std::list<int64_t>& chatIds, const std::wstring& msg,
                             bool disableWebPagePreview = false, int32_t replyToMessageId = 0,
                             TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                             const std::string& parseMode = "", bool disableNotification = false) = 0;

    virtual void SendMessage(int64_t chatId, const std::wstring& msg, bool disableWebPagePreview = false,
                             int32_t replyToMessageId = 0,
                             TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                             const std::string& parseMode = "", bool disableNotification = false) = 0;

    // returns bot events to handle everything itself
    virtual TgBot::EventBroadcaster& GetBotEvents() = 0;

    // get api bot
    virtual const TgBot::Api& GetBotApi() = 0;
};
typedef std::shared_ptr<ITelegramThread> ITelegramThreadPtr;

// create an instance of our class
inline DLLIMPORT_EXPORT
ITelegramThreadPtr CreateTelegramThread(const std::string& botToken,
                                        ITelegramAlerter* alertInterface = nullptr);

// create an instance of our class
inline DLLIMPORT_EXPORT
ITelegramThreadPtr CreateTelegramThread(const std::string& botToken,
                                        const TelegramErrorHandler& errorHandler = nullptr);

// create an instance of the telegram bot
inline DLLIMPORT_EXPORT
std::unique_ptr<TgBot::Bot> CreateTelegramBot(const std::string& botToken,
                                              const TgBot::HttpClient& client);

// handle update event from telegram channel
inline DLLIMPORT_EXPORT
void HandleTgUpdate(const TgBot::EventHandler& handler, TgBot::Update::Ptr update);
