#### log
source: log4cpp loggerEvent & loggerAppender & loggerFormatter & logger
    url: https://log4cpp.sourceforge.net/
    mark:

    ```bash 
        %% - a single percent sign
        %c - the category
        %d - the date\n Date format: The date format character may be followed by a date format specifier enclosed between braces. For example, %d{%H:%M:%S,%l} or %d{%d %m %Y %H:%M:%S,%l}. If no date format specifier is given then the following format is used: "Wed Jan 02 02:03:55 1980". The date format specifier admits the same syntax as the ANSI C function strftime, with 1 addition. The addition is the specifier %l for milliseconds, padded with zeros to make 3 digits.
        %m - the message
        %n - the platform specific line separator
        %p - the priority
        %t - thread id
        %f - the file name
        %l - the line
        %T - the tab
    ```

    ```flowchart LR
        LoggerEvent --> Logger 
        Logger --> FileLoggerAppender
        FileLoggerAppender --> LoggerFormatter2
        Logger --> ConsoleLoggerAppender
        ConsoleLoggerAppender --> LoggerFormatter1
    ```

#### orm
source: ormpp
    url: https://github.com/qicosmos/ormpp

#### config
source: toml
    url: https://github.com/skystrife/cpptoml

#### system api hook
source: 
    mark:
        in linux, using dlsym get address of a symbol in a shared object or executable and rewrite symbol
        using extern "C" , extern in c program

        ```c++
            #include <dlfcn.h>

            #define XX(name) \
                name##_f = (name##_func)dlsym(RTLD_NEXT, #name);
            #undef XX

            extern "C" {
                typedef unsigned int (*sleep_func)(unsigned int seconds);
                extern sleep_func sleep_f;
                // rewrite sleep function
                unsigned int sleep(unsigned int seconds) {
                    /* do something */
                    return 0;
                }
            }
        ```

#### html template
source: ginger
    url: https://github.com/melpon/ginger

#### html markdown editor
source: pandao
    url: https://pandao.github.io/editor.md

#### http define
source: http mothed & statue & mime types define
    url: https://github.com/nodejs/http-parser
        - https://github.com/nodejs/http-parser/blob/main/http_parser.h
            - HTTP_STATUS_MAP & HTTP_METHOD_MAP
         https://github.com/qicosmos/cinatra
        - https://github.com/qicosmos/cinatra/blob/93bd6b3e30fbdf9577d04d7be2e50dced9ce1356/include/cinatra/mime_types.hpp
            - MIME_TYPES
    code format:

        ```c++
            #define XXXX_MAP(XX)    \
                XX(XXX,XXX,XXX)
        ```

#### http parser
source: http request & response message parser
    url: https://github.com/mongrel2/mongrel2
        - https://github.com/mongrel2/mongrel2/blob/master/src/http11/http11_common.h
        - https://github.com/mongrel2/mongrel2/blob/master/src/http11/http11_parser.h
        - https://github.com/mongrel2/mongrel2/blob/master/src/http11/http11_parser.rl
        - https://github.com/mongrel2/mongrel2/blob/master/src/http11/httpclient_parser.h
        - https://github.com/mongrel2/mongrel2/blob/master/src/http11/httpclient_parser.rl
    ragel:
        - ragle xxx.rl -o xxx.cc

#### unit test
source: doctest
    url: https://github.com/onqtam/doctest

