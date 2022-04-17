Telegram library. Used to unify work with telegram bots from various applications

Inside the library, there is a separate thread that exchanges requests with the telegram server via longpull (it can be improved and use webhook, but this is not necessary at the moment). Allows you to set callbacks for bot commands, send messages to users, and much more. Written in C++17.

To work with Telegram API by usage of https://github.com/reo7sp/tgbot-cpp

Usage:
1. Create a telegram bot (https://tlgrm.ru/docs/bots) and get its token.
2. Load submodules:
	- git submodule init
	- git submodule update --recursive
3. Connect the library and header TelegramThread.h (see macros My...).
4. Create a telegram bot thread via CreateTelegramThread by passing the token to it.
5. Through the ITelegramThread and TgBot::EventBroadcaster interface, connect the command handlers for the telegram bot commands.
6. Make ITelegramThread::StartTelegramThread and enjoy.

Library dependencies by Nuget:
Boost (asio and all that follows) or Curl
Crypto
OpenSSL

P.S. Curl can be used instead of Boost, this requires
1. Uncomment #define HAVE_CURL in stdafx.h
2. Macro $(CurlIncludeDir) - Path to include Curl files