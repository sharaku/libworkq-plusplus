# libworkq++

libworkq++は Linux Kernel内の workque という仕組みを参考にアプリケーションに組み込んで簡単に使用できるように機能拡張等を行ったライブラリです.

## 利用方法

libworkq++はヘッダのみで機能します.
workq++.hppを使用したいソースコードへ#includeしてください.

## 依存関係

libworkq++ は C++11のみに依存します. このため, C++11以降をサポートするコンパイラがあればどの環境でもコンパイル可能です.
動作確認は Ubuntuにて行っています.


## タイマーの仕組み

タイマーはstd::chrono::steady_clockを使用して判断されます. よって, 多くのシステムの場合, 時刻を補正してもタイムアウト時間に影響はありません.


