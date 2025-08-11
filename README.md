# Crypto Arbitrage Bot (Binance, Bybit) 📈

A high-performance, real-time crypto arbitrage bot built in modern **C++20**, designed for both **paper trading** and (future) **live execution** across major exchanges.

> **Note:** This project currently supports **paper trading only**. Live trading support is planned.

---

## 🚀 Features

- Real-time order book data from **Binance** and **Bybit** USDT futures
- Fast, configurable arbitrage detection and execution logic
- Paper trading mode with fee simulation
- Modular, exchange-agnostic architecture
- Easy configuration via `config.json`
- Ready for extension to live trading (Binance, Bybit, dYdX)

---

## ⚙️ Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/DulajSP/crypto-arbitrage-bot.git
cd crypto-arbitrage-bot
cp config.sample.json config.json
```

### 2. Build Dependencies & Project

> **Requirements:**
>
> - CMake >= 3.20
> - Ninja
> - [vcpkg](https://github.com/microsoft/vcpkg) (for dependency management)

```bash
./build.sh
```

### 3. Run the Bot

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
| `mode`               | `"paper"` for simulation (live mode planned)                    |
| `fees`               | Total trading fee (e.g., 0.04 = 0.04%)                          |
| `maxPosUsd`          | Maximum position size in USD per exchange per symbol            |
| `symbols`            | List of symbols to monitor (must be supported by all exchanges) |
| `minSpreadPercent`   | Minimum percentage spread required to trigger a trade           |
| `rebalanceMinSpread` | Minimum spread for rebalancing                                  |
| `checkIntervalSec`   | How often (in seconds) to evaluate arbitrage opportunities      |

---

## ⚠️ Disclaimer

This project is provided **for educational and research purposes only**.

- **Live trading is not yet supported.**
- Trading cryptocurrencies involves significant risk and may result in **real financial loss**.
- The author provides **no guarantees** of accuracy, performance, or profit.
- You are fully responsible for testing, compliance, and risk management.
- Use of this software constitutes acceptance of the [MIT License](LICENSE).

---

## 📜 License

This project is licensed under the [MIT License](LICENSE).  
You are free to use, modify, and distribute it — with proper attribution.
