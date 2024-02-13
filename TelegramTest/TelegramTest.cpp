#include <atlstr.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <csignal>

#include <TelegramThread.h>

std::condition_variable gInterruptWork;

class TelegramAlerterHandler : public ITelegramAlerter
{
public:
    TelegramAlerterHandler() = default;

private:
    // оповещение о возникшей ошибке в работе телеграм бота
    void onAlertFromTelegram(const std::wstring& alertMessage) override
    {
        std::wcerr << L"Alert from telegram received: " << alertMessage << std::endl;
    }
};

void InitAndSetInterrupter()
{
    setlocale(LC_ALL, "Russian");
    std::cout << "Для выхода нажмите Ctrl+C/Ctrl+Break" << std::endl;

    auto stopApp = [](int)
    {
        gInterruptWork.notify_all();
    };

    signal(SIGINT, stopApp);
    signal(SIGBREAK, stopApp);
}

void WaitForExecution()
{
    std::mutex mutexWait;
    std::unique_lock<decltype(mutexWait)> mutexLock(mutexWait);
    gInterruptWork.wait(mutexLock);

    std::cout << "Завершено успешно" << std::endl;
}

std::list<ITelegramThread::CommandInfo> FillCommands(ITelegramThread* pTelegramThread)
{
    std::list<ITelegramThread::CommandInfo> commandsList;
    auto& obj = commandsList.emplace_back(ITelegramThread::CommandInfo{
        L"comand_1", L"Command1 description", [pTelegramThread](const TgBot::Message::Ptr message)
    {
        std::cout << "Получена команда Comand1" << std::endl;
        pTelegramThread->SendMessage(message->chat->id, L"Получена команда1");
    }});

    return commandsList;
}

void AddKeyboardWithCallbacks(ITelegramThread* pTelegramThread, std::list<ITelegramThread::CommandInfo>& commandsList)
{
    commandsList.emplace_back(ITelegramThread::CommandInfo{
        L"inline_keyboard",
        L"InlineKeyboard description",
        [pTelegramThread](const TgBot::Message::Ptr message)
    {
         std::cout << "Получена команда InlineKeyboard" << std::endl;

         TgBot::InlineKeyboardButton::Ptr button1 = std::make_shared<TgBot::InlineKeyboardButton>();
         button1->text = getUtf8Str(L"Button 1");
         button1->callbackData = button1->switchInlineQuery = "Callback Button 1";

         TgBot::InlineKeyboardButton::Ptr button2 = std::make_shared<TgBot::InlineKeyboardButton>();
         button2->text = getUtf8Str(L"Кнопка 2");
         button2->callbackData = button2->switchInlineQuery = "Callback Button 2";

         // показываем пользователю кнопки в выбором канала по которому нужен отчёт
         TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
         keyboard->inlineKeyboard.push_back({button1, button2});

         pTelegramThread->SendMessage(message->chat->id, L"Получена InlineKeyboard", false, false, keyboard);
    }});

    commandsList.emplace_back(ITelegramThread::CommandInfo{
        L"reply_keyboard",
        L"ReplyKeyboard description",
        [pTelegramThread](const TgBot::Message::Ptr message)
    {
         std::cout << "Получена команда ReplyKeyboard" << std::endl;

         TgBot::KeyboardButton::Ptr button1 = std::make_shared<TgBot::KeyboardButton>();
         button1->text = getUtf8Str(L"Передать свой контакт");
         button1->requestContact = true;

         TgBot::KeyboardButton::Ptr button2 = std::make_shared<TgBot::KeyboardButton>();
         button2->text = getUtf8Str(L"Передать местоположение");
         button2->requestLocation = true;

         // показываем пользователю кнопки в выбором канала по которому нужен отчёт
         TgBot::ReplyKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::ReplyKeyboardMarkup>();
         keyboard->keyboard.push_back({button1, button2});
         keyboard->resizeKeyboard = true;

         pTelegramThread->SendMessage(message->chat->id, L"Получена ReplyKeyboard", false, false, keyboard);
    } });

    commandsList.emplace_back(ITelegramThread::CommandInfo{
        L"remove_keyboard",
        L"RemoveKeyBoard description",
        [pTelegramThread](const TgBot::Message::Ptr message)
    {
         std::cout << "Получена команда RemoveKeyBoard" << std::endl;

         pTelegramThread->SendMessage(message->chat->id, L"Получена RemoveKeyBoard", false, false, std::make_shared<TgBot::ReplyKeyboardRemove>());
    } });

    pTelegramThread->GetBotEvents().onInlineQuery([pTelegramThread](const TgBot::InlineQuery::Ptr query)
    {
        const std::wstring text = L"onInlineQuery: " + getUNICODEString(query->query);

        std::wcout << text << std::endl;

        pTelegramThread->SendMessage(query->from->id, text);
    });

    pTelegramThread->GetBotEvents().onCallbackQuery([pTelegramThread](const TgBot::CallbackQuery::Ptr query)
    {
        std::wstring text = L"Нажата кнопка из: " + getUNICODEString(query->message->text) + L"\n";
        text += L"С колбэком: " + getUNICODEString(query->data);

        std::wcout << text << std::endl;

        pTelegramThread->SendMessage(query->message->chat->id, text);
    });

    pTelegramThread->GetBotEvents().onChosenInlineResult([pTelegramThread](const TgBot::ChosenInlineResult::Ptr result)
    {
        const std::wstring text = L"onChosenInlineResult: " + getUNICODEString(result->query);

        std::wcout << text << std::endl;

        pTelegramThread->SendMessage(result->from->id, text);
    });
}

int main()
{
    InitAndSetInterrupter();

    std::string tokenStr;
    if (std::ifstream tokenInfoFile("token.txt"); tokenInfoFile.is_open())
    {
        std::getline(tokenInfoFile, tokenStr);
    }
    else
    {
        std::cerr << "Не найден файл с токеном бота, ищем token.txt" << std::endl;
        return -3;
    }

    TelegramAlerterHandler alertHandler;
    ITelegramThreadPtr pTelegramThread = CreateTelegramThread(tokenStr, static_cast<ITelegramAlerter*>(&alertHandler));

    auto commands = FillCommands(pTelegramThread.get());

    AddKeyboardWithCallbacks(pTelegramThread.get(), commands);

    const CommandCallback onUnknownCommand = [&](const TgBot::Message::Ptr message)
    {
        std::wcout << L"Получена неизвестная команда и отправлена назад:\n" << getUNICODEString(message->text) << std::endl;

        pTelegramThread->SendMessage(message->chat->id, L"Получена неизвестная команда: " + getUNICODEString(message->text));
    };
    const CommandCallback onNonCommandMessage = [&](const TgBot::Message::Ptr message)
    {
        std::wcout << L"Получено неизвестное сообщение и отправлено назад:\n" << getUNICODEString(message->text) << std::endl;
        pTelegramThread->SendMessage(message->chat->id, L"Получено неизвестное сообщениe: " + getUNICODEString(message->text));
    };

    try
    {
        pTelegramThread->StartTelegramThread(commands, onUnknownCommand, onNonCommandMessage);

        WaitForExecution();
    }
    catch (std::exception& exception)
    {
        std::cout << "Не обработанное исключение: " << exception.what();
        return -1;
    }

    return 0;
}