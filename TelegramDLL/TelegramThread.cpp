/*
   Api description - https://core.telegram.org/bots/api
   token = token
   Chat ID - id
   Text - UTF-8

   Send message https://api.telegram.org/bot{token}/SendMessage?chat_id={chat_id}&text=test
   Check for updates https://api.telegram.org/bot{token}/getUpdates
*/

#include "stdafx.h"

#include <ext/trace/tracer.h>
#include <ext/std/string.h>
#include <ext/core/check.h>
#include <ext/core.h>

#include <atlconv.h>
#include <string>
#include <thread>
#include <unordered_map>

#include "TelegramThread.h"

using namespace TgBot;

// data structure for the thread to work
struct WorkTelegramData
{
    // thread flag
    std::atomic_bool bThreadWorkFlag = false;

    // bot
    Bot bot;

#ifdef HAVE_CURL
    // customer
    curlHttpClient curlHttpClient;
#endif //HAVE_CURL

    // callback to receive an error
    TelegramErrorHandler errorHandler;

    // constructor
    explicit WorkTelegramData(const std::string& token, TelegramErrorHandler errorHandlerFunction)
        : bot(token
#ifdef HAVE_CURL
              , curlHttpClient
#endif // HAVE_CURL
        )
        , errorHandler(std::move(errorHandlerFunction))
    {}
};

// in order not to stick out the inside of the bot, we make an auxiliary class
class TelegramThread : public ITelegramThread
{
public:
    // token - bot token
    explicit TelegramThread(const std::string& token, const TelegramErrorHandler& errorHandler = nullptr);

    ~TelegramThread();

// ITelegramThread
public:
    // start thread
    void StartTelegramThread(const std::unordered_map<std::string, CommandFunction>& commandsList,
                             const CommandFunction& onUnknownCommand = nullptr,
                             const CommandFunction& OnNonCommandMessage = nullptr) override;
    // stop thread
    void StopTelegramThread() override;

    // function for sending messages to chats
    void SendMessage(const std::list<int64_t>& chatIds, const std::wstring& msg, bool disableWebPagePreview = false, int32_t replyToMessageId = 0,
                     GenericReply::Ptr replyMarkup = std::make_shared<GenericReply>(), const std::string& parseMode = "", bool disableNotification = false) override;

    // function to send a message to the chat
    void SendMessage(int64_t chatId, const std::wstring& msg, bool disableWebPagePreview = false, int32_t replyToMessageId = 0,
                     GenericReply::Ptr replyMarkup = std::make_shared<GenericReply>(), const std::string& parseMode = "", bool disableNotification = false) override;

    // returns bot events to handle everything itself
    TgBot::EventBroadcaster& GetBotEvents() override;

    // get api bot
    const TgBot::Api& GetBotApi() override;
public:
    // telegram bot workflow
    std::thread m_telegramThread;

    // data required for the telegram to work
    WorkTelegramData m_telegramWorkData;
};

//----------------------------------------------------------------------------//
// send an error notification to the parent
void sendAlert(const TelegramErrorHandler& errorHandler, const char *format, ...);
void sendAlert(const TelegramErrorHandler& errorHandler, const std::wstring& str);

//----------------------------------------------------------------------------//
TelegramThread::TelegramThread(const std::string& token,
                               const TelegramErrorHandler& errorHandler /*= nullptr*/)
    : m_telegramWorkData(token, errorHandler)
{
    // Removing thousands separator from locale, awoid boost::lexical_cast wrong conversion
    const std::locale baseLoc = std::locale("");
    const auto localeWithFixedSeparators = std::locale(std::locale(baseLoc, new ext::core::numbers_formatter<wchar_t>()), new ext::core::numbers_formatter<char>());
    std::locale::global(localeWithFixedSeparators);
}

//----------------------------------------------------------------------------//
TelegramThread::~TelegramThread()
{
    StopTelegramThread();

    if (m_telegramThread.joinable())
        m_telegramThread.join();
}

// worker thread
// commandsList - list of commands and executable functions
// onAnyMessageCommand - code to be executed when any message is received
UINT telegramWorkThread(WorkTelegramData* telegramData)
{
    try
    {
        OutputDebugStringA(std::string_sprintf("Bot username: %s\n", telegramData->bot.getApi().getMe()->username.c_str()).c_str());
        telegramData->bot.getApi().deleteWebhook();
    }
    catch (std::exception& e)
    {
        sendAlert(telegramData->errorHandler, "Ошибка инициализации бота, описание ошибки: %s\n", e.what());
    }

    TgLongPoll longPoll(telegramData->bot);
    while (telegramData->bThreadWorkFlag)
    {
        try
        {
            OutputDebugStringA(std::string_sprintf("Long poll started\n").c_str());
            longPoll.start();
        }
        catch (std::exception& e)
        {
            if (telegramData->bThreadWorkFlag)
                break;

            OutputDebugStringA(std::string_sprintf("error: %s\n", e.what()).c_str());
            sendAlert(telegramData->errorHandler, "Ошибка в работе бота: %s\n", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    return 0;
}

//----------------------------------------------------------------------------//
void TelegramThread::StartTelegramThread(const std::unordered_map<std::string, CommandFunction>& commandsList,
                                         const CommandFunction& onUnknownCommand /*= nullptr*/,
                                         const CommandFunction& OnNonCommandMessage /*= nullptr*/)
{
    for (auto&& [command, function] : commandsList)
        m_telegramWorkData.bot.getEvents().onCommand(command, function);
    m_telegramWorkData.bot.getEvents().onUnknownCommand(onUnknownCommand);
    m_telegramWorkData.bot.getEvents().onNonCommandMessage(OnNonCommandMessage);

    m_telegramWorkData.bThreadWorkFlag = true;

    EXT_ASSERT(!m_telegramThread.joinable() && "Поток телеграма уже запущен!");
    m_telegramThread.swap(std::thread(&telegramWorkThread, &m_telegramWorkData));
}

//----------------------------------------------------------------------------//
void TelegramThread::StopTelegramThread()
{
    m_telegramWorkData.bThreadWorkFlag = false;
    m_telegramWorkData.errorHandler = nullptr;
}

//----------------------------------------------------------------------------//
void TelegramThread::SendMessage(const std::list<int64_t>& chatIds, const std::wstring& msg,
                                 bool disableWebPagePreview, int32_t replyToMessageId,
                                 GenericReply::Ptr replyMarkup, const std::string& parseMode,
                                 bool disableNotification)
{
    // send message to all users
    for (auto& chatId : chatIds)
    {
        try
        {
            m_telegramWorkData.bot.getApi().sendMessage(chatId, getUtf8Str(msg), disableWebPagePreview,
                                                        replyToMessageId, replyMarkup,
                                                        parseMode, disableNotification);
        }
        catch (std::exception& e)
        {
            OutputDebugStringA(std::string_sprintf("Error SendMessage: %s\n", e.what()).c_str());
            sendAlert(m_telegramWorkData.errorHandler, "Ошибка отправки сообщения: %s\n", e.what());
        }
    }
}

//----------------------------------------------------------------------------//
void TelegramThread::SendMessage(int64_t chatId, const std::wstring& msg,
                                 bool disableWebPagePreview, int32_t replyToMessageId,
                                 GenericReply::Ptr replyMarkup, const std::string& parseMode,
                                 bool disableNotification)
{
    std::list<int64_t> chatIds;
    chatIds.push_back(chatId);
    SendMessage(chatIds, msg, disableWebPagePreview,
                replyToMessageId, replyMarkup,
                parseMode, disableNotification);
}

//----------------------------------------------------------------------------//
TgBot::EventBroadcaster& TelegramThread::GetBotEvents()
{
    return m_telegramWorkData.bot.getEvents();
}

//----------------------------------------------------------------------------//
const TgBot::Api& TelegramThread::GetBotApi()
{
    return m_telegramWorkData.bot.getApi();
}

//----------------------------------------------------------------------------//
void sendAlert(const TelegramErrorHandler& errorHandler, const char *format, ...)
{
    if (!errorHandler)
        return;

    va_list arg;

    /* Compute length of original message */
    va_start(arg, format);
    const int len = vsnprintf(NULL, 0, format, arg);
    va_end(arg);

    /* Allocate space for original message */
    std::string str;
    str.resize(len + 1);

    /* Write original message to string */
    va_start(arg, format);
    vsnprintf(str.data(), len+1, format, arg);
    va_end(arg);

    errorHandler(std::wstring(ATL::CA2W(str.c_str())));
}

//----------------------------------------------------------------------------//
void sendAlert(const TelegramErrorHandler& errorHandler, const std::wstring& str)
{
    if (errorHandler)
        errorHandler(str);
}

inline DLLIMPORT_EXPORT std::string getUtf8Str(const std::wstring& str)
{
    return std::string(ATL::CW2A(str.c_str(), CP_UTF8));
}

//----------------------------------------------------------------------------//
inline DLLIMPORT_EXPORT std::wstring getUNICODEString(const std::string& utf8Str)
{
    return std::wstring(ATL::CA2W(utf8Str.c_str(), CP_UTF8));
    /*
    CString cstr;

    size_t utf8StrLen = strlen(utf8Str.c_str());

    if (utf8StrLen == 0)
    {
        return cstr;
    }

    const LPTSTR ptr = cstr.GetBuffer(utf8StrLen + 1);

#ifdef UNICODE
    // CString is UNICODE string so we decode
    int newLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8StrLen, ptr, utf8StrLen + 1);
    if (!newLen)
    {
        cstr.ReleaseBuffer(0);
        return false;
    }
#else
    WCHAR* buf = (WCHAR*)malloc(utf8StrLen);

    if (buf == NULL)
    {
        cstr.ReleaseBuffer(0);
        return false;
    }

    int newLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8StrLen, buf, utf8StrLen);
    if (!newLen)
    {
        free(buf);
        cstr.ReleaseBuffer(0);
        return false;
    }

    EXT_ASSERT(newLen < utf8StrLen);
    newLen = WideCharToMultiByte(
        CP_ACP, 0,
        buf, newLen, ptr, utf8StrLen
    );
    if (!newLen)
    {
        free(buf);
        cstr.ReleaseBuffer(0);
        return false;
    }

    free(buf);
#endif

    cstr.ReleaseBuffer(newLen);
    return cstr;*/
}

//----------------------------------------------------------------------------//
inline DLLIMPORT_EXPORT ITelegramThreadPtr CreateTelegramThread(const std::string& token,
                                                                ITelegramAlerter* alertInterface /*= nullptr*/)
{
    if (alertInterface)
        return std::make_unique<TelegramThread>(token, std::bind(&ITelegramAlerter::onAlertFromTelegram, alertInterface, std::placeholders::_1));
    else
        return std::make_unique<TelegramThread>(token);
}

//----------------------------------------------------------------------------//
inline DLLIMPORT_EXPORT ITelegramThreadPtr CreateTelegramThread(const std::string& token,
                                                                const TelegramErrorHandler& alertHandler /*= nullptr*/)
{
    return std::make_unique<TelegramThread>(token, alertHandler);
}

//----------------------------------------------------------------------------//
inline DLLIMPORT_EXPORT std::unique_ptr<TgBot::Bot> CreateTelegramBot(const std::string& token,
                                                                      const TgBot::HttpClient& client)
{
    return std::make_unique<TgBot::Bot>(token, client);
}

//----------------------------------------------------------------------------//
inline DLLIMPORT_EXPORT void HandleTgUpdate(const TgBot::EventHandler& handler,
                                            TgBot::Update::Ptr update)
{
    handler.handleUpdate(update);
}

/*
// CURL
#include "curl/curl.h"
size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void sendRequestByCurl()
{
    CURL* curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        const CStringW unicode1 = L"chat_id=358782858&text=тест"; // 'Alpha' and 'Omega'
        const CStringA utf8 = CW2A(unicode1, CP_UTF8);

        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL,
                         "https://api.telegram.org/bot{token}/SendMessage");
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, utf8);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}*/