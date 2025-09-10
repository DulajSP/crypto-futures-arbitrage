#include "common/Helper.hpp"

#include <cpr/cpr.h>

int64_t Helper::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string Helper::upper(const std::string& s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), ::toupper);
    return t;
}

std::string Helper::toHexString(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (unsigned char byte : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

std::string Helper::toFixed(double v, int digits) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(digits) << v;
    return oss.str();
}

std::string Helper::trimTrailingZeros(const std::string& s) {
    auto str = s;
    if (auto pos = str.find('.'); pos != std::string::npos) {
        while (!str.empty() && str.back() == '0') str.pop_back();
        if (!str.empty() && str.back() == '.') str.pop_back();
    }
    return str.empty() ? "0" : str;
}

double Helper::floorToStep(double v, double step) {
    if (step <= 0.0) return v;
    const double k = std::floor(v / step);
    return k * step;
}

std::string Helper::toDydxSymbol(const std::string& engineSymbol) {
    const std::string suffix = "USDT";
    if (engineSymbol.size() > suffix.size() &&
        engineSymbol.rfind(suffix) == engineSymbol.size() - suffix.size()) {
        std::string base = engineSymbol.substr(0, engineSymbol.size() - suffix.size());
        return base + "-USD";
    }
    if (engineSymbol.find('-') != std::string::npos) return engineSymbol;
    return engineSymbol;
}

std::string Helper::fromDydxSymbol(const std::string& dydxSymbol) {
    auto pos = dydxSymbol.find('-');
    if (pos != std::string::npos) {
        std::string base = dydxSymbol.substr(0, pos);
        std::string quote = dydxSymbol.substr(pos + 1);
        if (quote == "USD") quote = "USDT";
        return base + quote;
    }
    return dydxSymbol;
}

std::pair<bool, std::string> Helper::httpGet(const std::string& baseURL,
                                             const std::string& path,
                                             const std::string& query) {
    std::string url = baseURL + path;
    if (!query.empty()) url += "?" + query;

    auto resp = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
    if (resp.error) return {false, resp.error.message};
    if (resp.status_code / 100 != 2) return {false, resp.text};
    return {true, resp.text};
}

std::pair<bool, std::string> Helper::httpPost(
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& contentType
) {
    cpr::Header hdr;
    hdr["Content-Type"] = contentType;
    for (const auto& [key, value] : headers) {
        hdr[key] = value;
    }

    auto resp = cpr::Post(
        cpr::Url{url},
        hdr,
        cpr::Body{body},
        cpr::Timeout{1000}
    );

    if (resp.error) return {false, resp.error.message};
    if (resp.status_code / 100 != 2) return {false, resp.text};
    return {true, resp.text};
}