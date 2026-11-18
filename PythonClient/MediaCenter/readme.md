linux:
1、安装包：
sudo apt install python3-tk
sudo apt install python3-pil.imagetk
sudo apt install -y fonts-noto-cjk fonts-wqy-zenhei

2、中文设置：
生成中文 Locale：
sudo dpkg-reconfigure locales
在出现的界面中，找到并选中 zh_CN.UTF-8（按空格键选中，出现 * 号），同时建议也选中 en_US.UTF-8。然后将 en_US.UTF-8 设为默认，最后按 Tab 键选择 <OK> 确认。

3、开启自动启动：
mkdir -p ~/.config/autostart
vi ~/.config/autostart/media-player.desktop

[Desktop Entry]
Type=Application
Name=Media Player
Comment=TV style media center
Exec=/usr/bin/python3 /home/cxyazhou/work/MediaPlayer/media_player.py
Path=/home/cxyazhou/work/MediaPlayer
Terminal=false
X-GNOME-Autostart-enabled=true

windows：
1、安装包：
pip install Pillow