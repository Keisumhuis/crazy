/**
 * @file email.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_EMAIL_EMAIL_H____
#define ____CRAZY_EMAIL_EMAIL_H____

#include <memory>

namespace crazy::email {

    class Email {
    public:
        using Ptr = std::shared_ptr<Email>;
    };

}

#endif // ! ____CRAZY_EMAIL_EMAIL_H____