# Crypto Futures Arbitrage (Binance, Bybit, dydx) 📈

A high-performance, real-time crypto futures arbitrage engine built in modern **C++20**, designed for both **paper trading** and **live execution** across major exchanges.

---

## 🚀 Features

- Real-time order book data from **Binance** , **dydx** and **Bybit** USDT futures
- Fast, configurable arbitrage detection and execution logic
- **Live trading** and **paper trading** modes with fee simulation
- Modular, exchange-agnostic architecture
- Easy configuration via `config.json`

---

## 🌐 Supported Exchanges

This bot supports the following exchanges for both **paper trading** and **live trading**:

- **Binance** (USDT futures)
- **Bybit** (USDT futures)
- **dydx** (Perpetual contracts)

---

## ⚙️ Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/DulajSP/crypto-futures-arbitrage.git
cd crypto-futures-arbitrage
cp config.sample.json config.json
```

### 2. Configure API Keys

To enable live trading, you need to provide API keys for the supported exchanges in the `config.json` file:

```json
{
  "binance": {
    "api_key": "your_binance_api_key",
    "secret": "your_binance_secret_key"
  },
  "bybit": {
    "api_key": "your_bybit_api_key",
    "secret": "your_bybit_secret_key"
  },
  "dydx": {
    "subaccount_number": "your_dydx_subaccount_number",
    "mnemonic": "your_dydx_wallet_mnemonic"
  }
}
```

- **Binance**: Provide your API key and secret.
- **Bybit**: Provide your API key and secret.
- **dydx**: Provide your subaccount number and wallet mnemonic.

> **Note**: Ensure that your API keys have the necessary permissions for trading and accessing account data.

### 3. Build Dependencies & Project

> **Requirements:**
>
> - CMake >= 3.20
> - Ninja
> - [vcpkg](https://github.com/microsoft/vcpkg) (for dependency management)

```bash
./build.sh
```

### 4. Run the Bot

```bash
./build/bin/arbitrage_bot
```

---

## 🛠 Configuration (`config.json`)

Example:

```json
{
  "mode": "paper",
  "fees": 0.04,
  "maxPosUsd": 1000,
  "symbols": ["BTCUSDT", "ETHUSDT", "SOLUSDT", "AVAXUSDT"],
  "minSpreadPercent": 0.05,
  "rebalanceMinSpread": 0.02,
  "checkIntervalSec": 1
}
```

| Key                  | Description                                                     |
| -------------------- | --------------------------------------------------------------- |
| `mode`               | `"paper"` for simulation, `"live"` for live trading             |
| `fees`               | Total trading fee (e.g., 0.04 = 0.04%)                          |
| `maxPosUsd`          | Maximum position size in USD per exchange per symbol            |
| `symbols`            | List of symbols to monitor (must be supported by all exchanges) |
| `minSpreadPercent`   | Minimum percentage spread required to trigger a trade           |
| `rebalanceMinSpread` | Minimum spread for rebalancing                                  |
| `checkIntervalSec`   | How often (in seconds) to evaluate arbitrage opportunities      |

---

## ⚠️ Disclaimer

This project is provided **for educational and research purposes only**.

- Trading cryptocurrencies involves significant risk and may result in **real financial loss**.
- The author provides **no guarantees** of accuracy, performance, or profit.
- You are fully responsible for testing, compliance, and risk management.
- Use of this software constitutes acceptance of the [MIT License](LICENSE).

---

## 📜 License

This project is licensed under the [MIT License](LICENSE).  
You are free to use, modify, and distribute it — with proper attribution.
