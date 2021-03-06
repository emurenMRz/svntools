SVN dumpファイルビューア
===============================================================================

SVNのdumpファイルを閲覧するソフトウェアです。
--incrementalオプションや--deltasオプションが使用されたdumpファイルも読み込む事が出来ます。
バックアップしたdumpファイルの内容確認などにご利用ください。

64bit版Windows10 version 1903でのみ動作を確認しています。

## INSTALL / USING

インストールは必要ありません。
同梱のsvndumpviewer.exeを実行すると起動します。

起動したウィンドウにdumpファイルをドロップし読み込みます。
必要に応じて、メニュー内のチェックを切り替えて御利用ください。

ウィンドウ上部はdumpファイルに含まれるリビジョン一覧が表示されます。
一覧からリビジョンを選択すると、ウィンドウ下部左側にそのリビジョンで追加(add)・変更(change)・削除(delete)されたファイル(ノード)の一覧が表示されます。
ノードを選択すると、追加・変更ノードの場合はウィンドウ下部右側にそのファイルの内容※が表示されます。
また追加・変更ノードをダブルクリックすると、該当ファイルを出力する事が出来ます。

※Windows Imaging Componentが対応しているファイルは画像として、それ以外はテキストまたはHEX形式で表示されます。

## UNINSTALL

本ファイルと同梱のsvndumpviewer.exeを削除すればアンインストール完了です。
レジストリは使用していません。

## Changelog

2019/09/15 ver1.0 release: 初回公開

## COPYING / LICENSE 

Copyright (c) 2019, [emuren](emuren@mrz-net.org)

本ソフトウェアを使用した結果については全て自己責任でお願いします。

配布元: http://www.mrz-net.org/
連絡先: [Twitter]@emurenMRz / @mrz_net_org
再配布: 一切禁止します。
