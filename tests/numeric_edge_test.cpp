#include "json.hpp"

#include <cassert>
#include <cmath>

int main()
{
    {
        char input[] = "1e309";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(!result);
        assert(result.code == json::error::invalid_number);
    }

    {
        char input[] = "-1e309";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(!result);
        assert(result.code == json::error::invalid_number);
    }

    {
        char input[] = "1e-10000";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(result);
        assert(std::isfinite(document.root().as_number()));
    }

    return 0;
}
