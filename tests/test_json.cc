#include "crazy.h"

struct parent {
    std::string name;
    int32_t age;
};
REFLECTION(parent, name, age);
struct person {
    std::string name;
    int32_t age;
    std::vector<std::string> books; 
    parent father;
};
REFLECTION(person, name, age, books);

int main() {
    person _person;
    _person.name = "keisum";
    _person.age = 19;
    _person.books.push_back("C++");
    _person.father.name = "keisum'father";
    _person.father.name = 40;
    auto json_str = crazy::json::Converter::serialize(_person);
    CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << json_str;
    auto _person_dec = crazy::json::Converter::deserialize<person>(json_str);
    CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "name = " << _person_dec.name << " age = " << _person_dec.age;
}