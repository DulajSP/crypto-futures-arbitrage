#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <unordered_map>

class Helper {
public:
    // Time
    static int64_t nowMs();

    // String
    static std::string upper(const std::string& s);
    static std::string toHexString(const std::vector<unsigned char>& data);
    static std::string toFixed(double v, int digits);
    static std::string trimTrailingZeros(const std::string& s);

    // Math
    static double floorToStep(double v, double step);

    // Symbol conversion
    static std::string toDydxSymbol(const std::string& engineSymbol);
    static std::string fromDydxSymbol(const std::string& dydxSymbol);

    // Http
    static std::pair<bool, std::string> httpGet(const std::string& baseURL,
                                            const std::string& path,
                                            const std::string& query = "");

    static std::pair<bool, std::string> httpPost(
        const std::string& url,
        const std::string& body,
        const std::unordered_map<std::string, std::string>& headers,
        const std::string& contentType = "application/json"
    );

};