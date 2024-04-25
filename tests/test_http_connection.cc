#include "crazy.h"

int main() {
    auto res = crazy::http::HttpConnection::Get("https://ct4.tostar.top/CT4ViewOnline.html", -1);
    if (res->error != crazy::http::ResultError::OK) {
        CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "get uri = https://ct4.tostar.top/CT4ViewOnline.html fail = " << res->errstr;
        return 0; 
    }
    CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << res->response->GetBody();
}
