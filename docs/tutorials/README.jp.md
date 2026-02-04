# <p align="center">⚛️ PHOTON</p>
<p align="center">
  <i align="center">The Native Agentic Core — 次世代エンジニアリングのためのネイティブコア</i>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/Performance-Zero_Latency-FF6F00?style=for-the-badge" alt="Performance">
  <img src="https://img.shields.io/badge/Integration-Minimalist-673AB7?style=for-the-badge" alt="Integration">
</p>

<p align="center">
  <a href="https://github.com/QkHearn">
    <img src="https://img.shields.io/badge/Architect-Hearn-black?style=flat-square&logo=github" alt="Author">
  </a>
  <a href="mailto:hearn.qk@gmail.com">
    <img src="https://img.shields.io/badge/Contact-hearn.qk@gmail.com-red?style=flat-square&logo=gmail" alt="Email">
  </a>
</p>

---

<p align="center">
  <img src="../../demo.png" width="800" alt="Photon Demo">
</p>

---

## 💎 なぜ Photon なのか？ [Why Photon?]

### ⚡ 究極のネイティブ性能 (Native Velocity)
*   **ハードコア性能**: 純粋な C++17 で構築され、ツールチェーン呼び出しにおいて**マイクロ秒単位の応答**を実現。
*   **極低リソース占有**: 実行メモリはわずか **10MB〜30MB**。スクリプト言語より 10 倍以上高速に起動。
*   **ゼロ依存配布**: Python や Node.js などの実行環境は不要。単一バイナリで、あらゆるプラットフォームで「即利用可能」。

### 🔌 極限のシンプル統合 (Minimalist Integration)
*   **Drop-in & Run**: 複雑なパッケージインストールは不要。バイナリをダウンロードしてターミナルで起動するだけ。
*   **IDE インビジブルエンジン**: Cursor や VS Code などの IDE の背後で動作する「見えないインフラ」として設計。
*   **CLI ファースト**: 強力なコマンドラインインターフェースにより、自動化スクリプトや CI/CD パイプラインに「脳」を。

### 🧠 深いエンジニアリング知覚 (Engineering Perception)
*   **構造的認知**: 内蔵の AST 解析により、単なるテキストマッチングを超え、複雑なコードロジックを深く理解。
*   **原子レベルの介入**: すべてのコード修正は決定論的に検証され、ワンクリック `undo` をサポート。エンジニアリング状態の絶対的な安全を確保。
*   **専門家パラダイムの注入**: モジュール化された Skills システムを通じて、シニアエンジニアの設計経験を AI のネイティブな本能として固定化。

---

## 🏗️ システムアーキテクチャ [Architecture]

```text
                     ┌──────────────────────────────────────────┐
                     │           IDE / Terminal Host            │
                     │ (VS Code, JetBrains, Cursor, Vim, etc.)  │
                     └───────────────────┬──────────────────────┘
                                         │ (Standard I/O)
                     ┌───────────────────▼──────────────────────┐
                     │            PHOTON KERNEL (C++)           │
                     │  ──────────────────────────────────────  │
                     │  [ Context Manager ]    [ Memory System ]│
                     │  (Short-term Mem)       (JSON/Markdown)  │
                     └───────────┬────────────────────┬─────────┘
                                 │                    │
                    ┌────────────▼───────┐    ┌───────▼─────────────┐
                    │     LLM Client     │    │  MCP Tool Manager   │
                    │   (Reasoning Hub)  │    │   (Execution Hub)   │
                    └────────────────────┘    └───────┬─────────────┘
                                                      │
          ┌───────────────────────────────────────────┴──────────────────────────┐
          │                                    │                                 │
┌─────────▼──────────┐               ┌─────────▼──────────┐            ┌─────────▼──────────┐
│   Built-in Tools   │               │   Expert Skills    │            │    External MCP    │
│    (C++ Native)    │               │  (Modular Logic)   │            │   (Node / Python)  │
├────────────────────┤               ├────────────────────┤            ├────────────────────┤
│• Parallel Search   │               │• Code Architect    │            │• Puppeteer         │
│• AST Analysis      │               │• Debug Master      │            │• Google Search     │
│• Python Sandbox    │               │• Env Initializer   │            │• Custom Servers    │
└────────────────────┘               └────────────────────┘            └────────────────────┘
```

---

## 🧠 コア概念の解説 [Core Concepts]

### 🛠️ Tools：エージェントの「デジタル触手」 (Actuation Layer)
> **ディープ・パースペクティブ**: AI 領域において、知覚と実行はエージェントが物理世界と対話する唯一の手段です。Tools のない AI は単なるチャットボックスですが、Tools を持つ Photon は「動けるエンジニア」です。

*   **全息感知**: 高性能な並列検索と AST 解析を内蔵。トークンを消費せずに大規模プロジェクトの構造を瞬時に分解。
*   **原子レベルの介入**: 決定論的なコード注入（Diff Apply）と隔離されたサンドボックスにより、すべての実行を正確かつ安全に。
*   **無限の拡張性**: **MCP (Model Context Protocol)** に基づき、世界中のコミュニティが提供する数千のツールサーバーと連携可能。

### 📜 Skills：デジタル生命の「専門家本能」 (Expert Paradigms)
> **ディープ・パースペクティブ**: 汎用的な LLM は特定のエンジニアリング領域における深みに欠けることがあります。Skills システムは「経験」を「本能」に変えることで、この溝を埋めます。

*   **思考の固定化**: モジュール化された `SKILL.md` を通じて、シニアアーキテクトの思考プロセス（例：性能チューニング、高並列システム設計）を AI の意思決定層に直接注入。
*   **能動的な進化**: AI は実行前に自ら Skill ガイドを読み込み、単なる確率予測ではなく、工業規格に準拠したエンジニアリングを実践します。

### 💾 Memory：時空を超える「自己の連続性」 (Neural Persistence)
> **ディープ・パースペクティブ**: 記憶のない AI は単なる関数呼び出しですが、記憶のある AI は進化する生命です。

*   **多態的記憶システム**: 高速検索のための JSON 断片事実と、深い理解のための Markdown 構造化知識を統合。
*   **習慣の同期**: Photon はプロジェクトの規約、過去のバグの教訓、特定のコーディング嗜好を記録し続けます。使い込むほどに、AI はあなたと「同期」していきます。

### 📑 Context：高次元の「思考ワークスペース」 (Cognitive Workspace)
> **ディープ・パースペクティブ**: LLM のコンテキストウィンドウは高価で有限です。Context Manager は Photon の「脳の執事」です。

*   **インテリジェントな圧縮**: 対話が膨大になった際、重要な意思決定ポイントを自動識別し、LLM を用いて再帰的に要約・圧縮。
*   **ロスレスな思考**: 核心的なロジックを維持したまま思考の有効長を劇的に延長し、数万行規模の巨大なタスクの処理を可能にします。

---

## 🚀 クイックスタート [Quick Start]

### 1. ダウンロード
[Releases](https://github.com/QkHearn/Photon/releases) ページから、お使いの OS に適したバイナリをダウンロードします。

### 2. 設定
プログラムと同じディレクトリに `config.json` を作成または編集し、API キーを設定します：

```json
{
  "llm": {
    "api_key": "YOUR_API_KEY",
    "base_url": "https://api.moonshot.cn/v1",
    "model": "kimi-k2-0905-preview"
  }
}
```

### 3. 起動
ターミナルで実行し、プロジェクトのディレクトリを指定します：

```bash
# 実行権限の付与 (Linux/macOS)
chmod +x photon

# 分析の開始
./photon /path/to/your/project
```

---

<p align="center">
  <a href="../../README.md">English</a> | <a href="README.zh.md">中文</a> | <a href="README.jp.md">日本語</a>
</p>

<p align="center">
  <i>"Photon: The bridge between human intent and machine logic, at the speed of light."</i>
</p>
