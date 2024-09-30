#include <minja.hpp>
#include <iostream>

using json = nlohmann::ordered_json;

int main() {
    auto tmpl = minja::Parser::parse("Hello, {{ location }}!", /* options= */ {});
    auto context = minja::Context::make(minja::Value(json {
        {"location", "World"},
    }));
    auto result = tmpl->render(context);
    std::cout << result << std::endl;
}
