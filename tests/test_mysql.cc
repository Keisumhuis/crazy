#include "crazy.h"
#include <type_traits>
#include <tuple>
#include <string_view>
#include <array>
#include <cstdint>

int main() {
    auto connection = std::make_shared<crazy::MySQLConnection>();
    if (!connection->connect("127.0.0.1", "root", "123456")) {
        CRAZY_ROOT_ERROR() << "connect mysql fail, error = " << connection->get_errno()
            << ", error message = " << connection->get_error_message();
        return -1;
    }
    auto result = connection->exec("select current_timestamp()");
    result = connection->exec_fmt("select * from mysql.db");
    if (!result) {
        return -1;
    }
    while (auto row = result->row()) {
        for (size_t i = 0; i < result->fields_count(); ++i) {
            CRAZY_ROOT_DEBUG() << row[i];
        }
    }
    return 0;
}
