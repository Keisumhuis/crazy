/**
 * @file aspect.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_HTTP_ASPECT_H____
#define ____CRAZY_HTTP_ASPECT_H____

#include <memory>

#include "crazy/http/request.h"
#include "crazy/http/response.h"

namespace crazy::http {

	struct AspectInterface {
		using Ptr = std::shared_ptr<AspectInterface>;
		virtual ~AspectInterface() {}
		virtual bool Before(Request::Ptr req, Response::Ptr res) = 0;
		virtual bool After(Request::Ptr req, Response::Ptr res) = 0;
	};

}

#endif // ! ____CRAZY_HTTP_ASPECT_H____
