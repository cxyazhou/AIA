import socket
import threading
import json
import time
import subprocess
import os
import sys
import glob
import telnetlib
from typing import Optional, List, Tuple
from datetime import datetime

class MessageType:
    REGISTER = "register"
    GET_CLIENTS = "get_clients"
    SEND_MESSAGE = "send_message"
    BROADCAST = "broadcast"
    PRIVATE_MESSAGE = "private"
    HEARTBEAT = "heartbeat"
    RESPONSE = "response"

class Message:
    def __init__(self, msg_type: str, data: dict, sender: str = None, target: str = None, timestamp: str = None, msg_id: int = 0):
        self.id = msg_id  # 请求ID：服务器下发命令时携带，客户端回传结果时原样返回
        self.type = msg_type
        self.data = data
        self.sender = sender
        self.target = target
        self.timestamp = timestamp or datetime.now().isoformat()
    
    def to_json(self) -> str:
        obj = {
            'type': self.type,
            'data': self.data,
            'sender': self.sender,
            'target': self.target,
            'timestamp': self.timestamp
        }
        if self.id:
            obj['id'] = self.id
        return json.dumps(obj)
    
    @staticmethod
    def from_json(json_str: str) -> 'Message':
        try:
            data = json.loads(json_str)
            return Message(
                msg_type=data.get('type'),
                data=data.get('data', {}),
                sender=data.get('sender'),
                target=data.get('target'),
                timestamp=data.get('timestamp'),
                msg_id=data.get('id', 0)
            )
        except:
            return None

class TCPClient:
    def __init__(self, host: str = '127.0.0.1', port: int = 8180, heartbeat_interval: int = 60):
        self.host = host
        self.port = port
        self.socket = None
        self.name: Optional[str] = None
        self.running = False
        self.clients_list = []
        self.lock = threading.Lock()
        self.heartbeat_interval = heartbeat_interval
        self.heartbeat_thread = None
        self.message_queue = []
        self.buffer = ""

    def connect(self, name: str) -> bool:
        """连接到服务器（支持域名）"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            
            # 设置连接超时
            self.socket.settimeout(10)
            
            print(f"{name} 正在连接 {self.host}:{self.port}...")
            
            # socket的connect方法会自动解析域名
            self.socket.connect((self.host, self.port))
            
            # 取消超时设置
            self.socket.settimeout(None)
            
            self.name = name
            self.running = True
            
            # 启动接收消息的线程
            receive_thread = threading.Thread(target=self.receive_messages)
            receive_thread.daemon = True
            receive_thread.start()

            # 启动处理消息的线程
            process_thread = threading.Thread(target=self.process_messages)
            process_thread.daemon = True
            process_thread.start()
            
            # 启动心跳线程
            self.heartbeat_thread = threading.Thread(target=self.send_heartbeat)
            self.heartbeat_thread.daemon = True
            self.heartbeat_thread.start()
            
            # 注册客户端
            return self.register()
            
        except socket.gaierror:
            print(f"{self.name} 域名解析失败: {self.host}")
            return False
        except socket.timeout:
            print(f"{self.name} 连接超时: {self.host}:{self.port}")
            return False
        except ConnectionRefusedError:
            print(f"{self.name} 连接被拒绝，请检查服务器是否运行在 {self.host}:{self.port}")
            return False
        except Exception as e:
            print(f"{self.name} 连接失败: {e}")
            return False
    
    def register(self) -> bool:
        """注册客户端：上报可执行命令列表 + 连接密码"""
        data = {
            "name": self.name,
            "cmd": "显示文本",
            "password": "MyAIAServer"
        }
        message = Message(MessageType.REGISTER, data, sender=self.name, target="server")
        self.send_message(message)
        return True
    
    def receive_messages(self):
        """接收消息的线程"""
        while self.running:
            try:
                # 设置1秒超时，让接收线程可以定期醒来检查 self.running 状态，实现优雅退出
                self.socket.settimeout(1.0)
                data = self.socket.recv(4096).decode('utf-8')

                if not data:
                    print(f"{self.name} 收到空数据，连接可能已关闭")
                    break
                
                # 将新数据添加到缓冲区
                self.buffer += data
                
                # 处理缓冲区中的所有完整消息
                while '\n' in self.buffer:
                    # 找到第一个换行符位置
                    newline_pos = self.buffer.index('\n')
                    # 提取消息
                    message_json = self.buffer[:newline_pos]
                    # 移除已处理的消息
                    self.buffer = self.buffer[newline_pos + 1:]
                    
                    # 跳过空消息
                    if not message_json.strip():
                        print(f"{self.name} 收到空消息")
                        continue
                    
                    message = Message.from_json(message_json)
                    if message:
                        print(f"{self.name} 解析消息: type={message.type}, data={message.data} from {message.sender} to {message.target}, timestamp={message.timestamp}")
                        self.add_message(message)
                    else:
                        print(f"{self.name} 收到无效消息: {message_json}")
                                    
            except (ConnectionResetError, BrokenPipeError):
                break
            except socket.timeout:
                continue
            except Exception as e:
                print(f"接收消息出错: {e}")
                break
        
        self.running = False
    
    def add_message(self, message: Message):
        """添加私聊消息到消息队列"""
        self.message_queue.append(message)

    def process_messages(self):
        """处理私聊消息，完成"""
        while self.running:
            if self.message_queue:
                message = self.message_queue.pop(0)
                self.process_message(message)
            else:
                time.sleep(0.1)
    
    def process_message(self, message: Message):
        """处理接收到的消息"""
        print(f"Base Client Process message, Please Override this method")
        if message.type == MessageType.GET_CLIENTS:
            # 直接处理客户端列表
            clients = message.data.get('clients', [])
            with self.lock:
                self.clients_list = clients
            print(f"\n当前在线用户 ({len(clients)}): {', '.join(clients) if clients else '暂无其他用户'}")
            
        elif message.type == MessageType.RESPONSE:
            # 处理响应
            data = message.data
            if data.get('success'):
                if 'message' in data:
                    print(f"\n[系统] {data['message']}")
                # 如果响应中包含客户端列表数据
                if 'data' in data and data['data'] and 'clients' in data['data']:
                    clients = data['data']['clients']
                    with self.lock:
                        self.clients_list = clients
                    print(f"当前在线用户: {', '.join(clients) if clients else '暂无其他用户'}")
            else:
                print(f"\n[错误] {data.get('message', '未知错误')}")
                
        elif message.type == MessageType.PRIVATE_MESSAGE:
            # 收到私聊消息
            print(f"\n[私聊] {message.sender}: {message.data.get('content')}")
            
        elif message.type == MessageType.BROADCAST:
            # 收到广播消息
            print(f"\n[广播] {message.sender}: {message.data.get('content')}")

    def send_heartbeat(self):
        """发送心跳包，保持长连接"""
        while self.running:
            try:
                time.sleep(self.heartbeat_interval)
                if not self.running:
                    break
                message = Message(MessageType.HEARTBEAT, {}, sender=self.name, target="server")
                self.send_message(message)
            except Exception as e:
                if self.running:
                    print(f"{self.name} 发送心跳包失败：{e}")
                break
    
    def send_message(self, message: Message):
        """发送消息"""
        try:
            json_str = message.to_json() + '\n'
            self.socket.send(json_str.encode('utf-8'))
        except Exception as e:
            print(f"发送消息失败: {e}")
    
    def get_clients(self):
        """获取所有客户端列表"""
        message = Message(MessageType.GET_CLIENTS, {}, sender=self.name, target="server")
        self.send_message(message)
    
    def send_private_message(self, target: str, content: str):
        """发送私聊消息"""
        message = Message(
            MessageType.SEND_MESSAGE,
            {"content": content},
            sender=self.name,
            target=target
        )
        self.send_message(message)
    
    def broadcast(self, content: str):
        """广播消息"""
        message = Message(MessageType.BROADCAST, {"content": content}, sender=self.name, target="all_users")
        self.send_message(message)
    
    def close(self):
        """关闭连接"""
        self.running = False
        
        # 等待心跳线程结束（最多等待 2 秒）
        if self.heartbeat_thread and self.heartbeat_thread.is_alive():
            self.heartbeat_thread.join(timeout=2)
        
        if self.socket:
            try:
                self.socket.shutdown(socket.SHUT_RDWR)
            except:
                pass
            try:
                self.socket.close()
            except:
                pass
            self.socket = None

# ============================================================
# main_ubuntu.cpp handleMsg / handleCompile 的 Python 实现
# ============================================================

# 默认代码路径（对应 C++ 中的 code_path）
CODE_PATH = "/media/sf_F_DRIVE/IPC_QVCLOUD_NEW"
CODE_PATH_IOT = "/media/sf_F_DRIVE/PrjIotV2"

# IPC 设备 Telnet 重启参数
IPC_TELNET_HOST = "192.168.1.10"
IPC_TELNET_PORT = 23
IPC_TELNET_USER = "root"
IPC_TELNET_PASS = "qv2008"
IPC_TELNET_TIMEOUT = 10  # 秒


def telnet_reboot_ipc() -> bool:
    """通过 Telnet 登录 IPC 设备并执行 reboot。
    设备执行 reboot 后通常会主动断开连接，因此断链（EOF/ConnectionReset）
    在此场景下视为成功信号。
    """
    host = IPC_TELNET_HOST
    port = IPC_TELNET_PORT
    user = IPC_TELNET_USER.encode('utf-8')
    password = IPC_TELNET_PASS.encode('utf-8')
    tn: Optional[telnetlib.Telnet] = None
    try:
        print(f"Telnet 连接 {host}:{port} ...", flush=True)
        tn = telnetlib.Telnet(host, port, timeout=IPC_TELNET_TIMEOUT)

        # 等待登录提示符（兼容多种提示：login:/Login:/UserName:/全角冒号）
        COLON = rb'(?::|\xef\xbc\x9a)'  # 半角冒号 或 UTF-8 全角冒号
        idx, _, _ = tn.expect(
            [rb'[Ll]ogin\s*' + COLON + rb'\s*$',
             rb'[Uu]ser\s*[Nn]ame\s*' + COLON + rb'\s*$'],
            timeout=IPC_TELNET_TIMEOUT
        )
        if idx == -1:
            # 某些设备在连接建立后立即要求输入用户名，无显式 login 提示
            pass
        tn.write(user + b"\n")

        # 等待密码提示符
        idx, _, _ = tn.expect(
            [rb'[Pp]ass\s*[Ww]ord\s*' + COLON + rb'\s*$',
             rb'[Pp]asswd\s*' + COLON + rb'\s*$'],
            timeout=IPC_TELNET_TIMEOUT
        )
        if idx == -1:
            # 有些系统跳过密码提示（不太可能），继续尝试
            pass
        tn.write(password + b"\n")

        # 等 shell 提示符或登录成功标志（宽松处理：等待任意输出 1-2 秒）
        time.sleep(1.5)
        try:
            tn.read_very_eager()
        except Exception:
            pass

        print(f"Telnet 登录 {host} 成功，发送 reboot 命令", flush=True)
        tn.write(b"reboot\n")
        tn.write(b"killall -15 Sofia\n")

        # reboot 会触发设备主动断开，给一点时间并吞掉断链异常
        try:
            # 尝试读取剩余输出，最多等 3 秒
            tn.read_all()
        except (EOFError, ConnectionResetError, OSError, socket.error):
            # 预期的断链（设备已经重启）
            pass
        except Exception:
            pass

        try:
            tn.close()
        except Exception:
            pass
        print(f"Telnet 已向 {host} 发送 reboot", flush=True)
        return True
    except Exception as e:
        # reboot 发送后设备断开也可能在 expect/write 阶段触发异常，
        # 只要不是连接阶段的异常，通常都视为已生效。但这里统一打印日志。
        print(f"Telnet 重启 {host} 异常（可能已生效）: {e}", flush=True)
        if tn is not None:
            try:
                tn.close()
            except Exception:
                pass
        return False

# 上一次成功解析到的数字（对应 C++ 中的 g_lastCompileNumber）
_g_last_compile_number: Optional[str] = None

# 当前编译目标模式（默认 IPC），通过 "编译iot"/"编译ipc" 切换
_g_current_target = "ipc"

# 每种目标的动作字母：compile=编译, clean=清理, pack=打包
TARGET_ACTIONS = {
    "ipc": {"compile": "r", "clean": "c", "pack": "b"},
    "iot": {"compile": "r", "clean": "c", "pack": "b"},
}

# 中文数字映射（零一二三四五六七八九幺）
_CN_DIGIT_MAP = {
    0x96F6: '0',  # 零
    0x4E00: '1',  # 一
    0x5E7A: '1',  # 幺
    0x4E8C: '2',  # 二
    0x4E09: '3',  # 三
    0x56DB: '4',  # 四
    0x4E94: '5',  # 五
    0x516D: '6',  # 六
    0x4E03: '7',  # 七
    0x516B: '8',  # 八
    0x4E5D: '9',  # 九
}


def validate_num_part(s: str) -> bool:
    """校验命令前缀后的数字部分是否只含合法字符：
    - ASCII：仅允许数字、字母
    - UTF-8：仅允许中文数字字符（零一二三四五六七八九幺）
    空字符串视为合法（表示复用上一次数字）。
    返回 True 表示合法，False 表示含非法字符。
    """
    i = 0
    while i < len(s):
        c = s[i]
        cp = ord(c)
        if cp < 0x80:
            if not (c.isdigit() or c.isalpha()):
                return False
            i += 1
        else:
            # Python 字符串已是 Unicode，直接对照中文数字码点
            if cp not in _CN_DIGIT_MAP:
                return False
            i += 1
    return True


def normalize_digits_to_arabic(s: str) -> str:
    """将字符串中的中文数字逐位转为阿拉伯数字字符，
    已是阿拉伯数字/字母的保持不变（字母调用方转大写）。
    """
    out_chars = []
    for c in s:
        cp = ord(c)
        if cp < 0x80:
            if c.isdigit() or c.isalpha():
                out_chars.append(c)
        else:
            d = _CN_DIGIT_MAP.get(cp)
            if d:
                out_chars.append(d)
    return ''.join(out_chars)


class Step:
    __slots__ = ('cmd_type', 'ok', 'fail')

    def __init__(self, cmd_type: str, ok: str, fail: str):
        self.cmd_type = cmd_type  # "compile", "clean", "pack"
        self.ok = ok
        self.fail = fail


class PrefixAction:
    __slots__ = ('prefix', 'steps')

    def __init__(self, prefix: str, steps: List[Step]):
        self.prefix = prefix
        self.steps = steps


# 前缀动作表（注意："编译程序" 必须在 "编译" 之前匹配）
PREFIX_ACTIONS: List[PrefixAction] = [
    PrefixAction("编译程序", [Step("compile", "编译成功", "编译失败")]),
    PrefixAction("编译",     [Step("compile", "编译成功", "编译失败")]),
    PrefixAction("跑一下",   [Step("compile", "编译成功", "编译失败")]),
    PrefixAction("重新编译", [Step("clean", "清理成功", "清理失败"),
                              Step("compile", "编译成功", "编译失败")]),
    PrefixAction("清理",     [Step("clean", "清理成功", "清理失败")]),
    PrefixAction("打包",     [Step("pack", "打包成功", "打包失败")]),
]


def _run_system(cmd: str) -> int:
    """等价于 C++ std::system：通过 shell 执行命令并返回退出码。"""
    print(f"调用 system: {cmd}", flush=True)
    try:
        rc = subprocess.call(cmd, shell=True)
    except Exception as e:
        print(f"system 执行异常: {e}", flush=True)
        return -1
    print(f"system 返回值: {rc}", flush=True)
    return rc


def handle_compile(content: str) -> int:
    """返回 0 表示处理完成（即便未匹配前缀也返回 0），返回 -1 表示错误。

    模式切换：
      "编译iot" → 切换到 IOT 模式（code_path=PrjIotV2, 命令=./make.sh r 717457）
      "编译ipc" → 切换到 IPC 模式（code_path=IPC_QVCLOUD_NEW, 命令=./make.sh QV-IPC-717457 r）
    编译/运行等命令使用当前模式，默认 IPC。
    """
    global _g_last_compile_number, _g_current_target

    # ---- 模式切换命令（优先匹配，无需数字） ----
    if content.startswith("编译iot"):
        _g_current_target = "iot"
        msg = "已切换到 IOT 模式 (PrjIotV2)"
        print(msg, flush=True)
        return 0
    if content.startswith("编译ipc"):
        _g_current_target = "ipc"
        msg = "已切换到 IPC 模式 (IPC_QVCLOUD_NEW)"
        print(msg, flush=True)
        return 0

    # ---- 匹配编译/清理/打包前缀 ----
    prefix = ""
    matched_steps: List[Step] = []
    for a in PREFIX_ACTIONS:
        if content.startswith(a.prefix):
            prefix = a.prefix
            matched_steps = a.steps
            break

    if not prefix:
        # 未匹配前缀，不属于编译指令
        return 0

    num_part = content[len(prefix):]
    if not validate_num_part(num_part):
        print(f"{prefix}指令后包含非法字符（仅允许数字、字母、中文数字）: {num_part}", flush=True)
        return -1

    arabic = normalize_digits_to_arabic(num_part).upper()

    if not arabic:
        if not _g_last_compile_number:
            print(f"{prefix}指令未解析出数字，且无历史数字可用: {num_part}", flush=True)
            return -1
        arabic = _g_last_compile_number
        print(f"{prefix}指令未带数字，复用上次数字: {arabic}", flush=True)
    else:
        _g_last_compile_number = arabic
        print(f"{prefix}指令：原始数字片段={num_part} -> 阿拉伯数字={arabic}", flush=True)

    # 根据当前模式选择代码路径与命令格式
    target = _g_current_target
    code_path = CODE_PATH_IOT if target == "iot" else CODE_PATH
    product_prefix = "QV-IOT" if target == "iot" else "QV-IPC"
    actions = TARGET_ACTIONS.get(target, TARGET_ACTIONS["ipc"])
    if target == "iot":
        # IOT: ./make.sh <action> <number>
        make_cmd = lambda cmd_type: f"./make.sh {actions[cmd_type]} {arabic}"
    else:
        # IPC: ./make.sh QV-IPC-<number> <action>
        make_cmd = lambda cmd_type: f"./make.sh QV-IPC-{arabic} {actions[cmd_type]}"

    print(f"当前模式: {target.upper()}, 代码路径: {code_path}", flush=True)

    # 打包前：先删除旧版本目录
    if prefix == "打包":
        for pattern in (
            f"/media/sf_F_DRIVE/0_version/{product_prefix}-{arabic}*",
            f"/tftpboot/{product_prefix}-{arabic}*",
        ):
            matched = glob.glob(pattern)
            if matched:
                rm_cmd = f'rm -rf {" ".join(matched)}'
                _run_system(rm_cmd)

    # 循环执行步骤
    step_count = len(matched_steps)
    all_steps_ok = True
    for i in range(step_count):
        s = matched_steps[i]
        cmd = f"source /etc/profile 2>/dev/null; cd {code_path}; {make_cmd(s.cmd_type)}"
        cmd_send_msg = f"{code_path}: {make_cmd(s.cmd_type)}"
        rc = _run_system(cmd)
        if rc == 0:
            print(f"{s.ok} {arabic}", flush=True)
        else:
            print(f"{s.fail} {arabic}", flush=True)
            all_steps_ok = False
            break  # 失败则停止后续步骤

    # 全部步骤成功后的额外动作
    if all_steps_ok:
        if prefix == "跑一下":
            cp_cmd = f'chmod 777 /tftpboot/Sofia'
            _run_system(cp_cmd)
            telnet_reboot_ipc()
        elif prefix == "打包":
            src_pattern = f"/tftpboot/{product_prefix}-{arabic}*"
            matched = glob.glob(src_pattern)
            if matched:
                cp_cmd = f'cp -r {" ".join(matched)} /media/sf_F_DRIVE/0_version/'
                _run_system(cp_cmd)

    return 0


def handle_msg(sender: Optional[str], content: str) -> str:
    """处理消息，返回最终结果文本。"""
    ret = handle_compile(content)
    return ret;


# 本客户端支持的可执行命令（与注册时上报的 cmd 一致）
EXECUTABLE_COMMANDS = ["编译", "跑一下", "编译程序", "重新编译", "清理", "打包"]

# 与服务器 ExecutableCommand::TEXT_DISPLAY 一致：本客户端注册的能力
TEXT_DISPLAY_CMD = "显示文本"


def extract_command_after_received(content: str) -> str:
    """如果消息里带有“收到”前缀，则从“收到”后面截取真正的命令内容。"""
    if not content:
        return content
    pos = content.find("收到")
    if pos == -1:
        return content
    candidate = content[pos + 2:].strip()
    return candidate if candidate else content


def is_compile_command(content: str) -> bool:
    """判断私信内容是否为编译类命令（含模式切换命令）。"""
    if not content:
        return False
    if content.startswith("编译iot") or content.startswith("编译ipc"):
        return True
    return any(content.startswith(a.prefix) for a in PREFIX_ACTIONS)


class UbuntuTCPClient(TCPClient):
    """作为文本显示器（显示文本）注册到服务器：
    1. 服务器下发"显示文本"命令 → 解析文本 → 本地显示 → 带原 id 回传结果
    2. 收到私信 → 内容为编译命令则走 handleMsg 编译流程，否则按文本显示处理
       → 处理结果通过 RESPONSE 发回给服务器
    """

    def send_msg(self, target: Optional[str], msg: str) -> None:
        """对应 C++ TcpClientSendMsg(name, msg)：name=None 广播，否则私聊。"""
        if target is None:
            self.broadcast(msg)
        else:
            self.send_private_message(target, msg)

    def _reply_result(self, msg_type: str, target: str, result: str, msg_id: int) -> None:
        """命令处理完后把结果回传（带服务器下发的原 id）。"""
        resp = Message(
            msg_type,
            {'文本': result},
            sender=self.name,
            target=target,
            msg_id=msg_id
        )
        self.send_message(resp)
        print(f"回复服务器 (id={msg_id}): {result}", flush=True)

    # ---- override ----
    def process_message(self, message: Message):
        """重写：get_clients / response 走父类逻辑；
        服务器下发的"显示文本"命令解析后显示并回传结果；
        私信中的命令处理后把结果发回服务器。
        """
        if message.type == MessageType.GET_CLIENTS:
            super().process_message(message)
        elif message.type == MessageType.RESPONSE:
            super().process_message(message)
        elif message.type == TEXT_DISPLAY_CMD:
            # 服务器下发的命令
            content = message.data.get('文本', '') or message.data.get('content', '')
            content = extract_command_after_received(content)
            print(f"\n[指令] {message.sender}: {content}", flush=True)
            
            if is_compile_command(content):
                resp = Message(
                    MessageType.RESPONSE,
                    {'客户端名称': self.name, '文本': "开始执行" + content},
                    sender=self.name,
                    target="server"
                )
                self.send_message(resp)
                
                ret = handle_msg(message.sender, content)
                if ret == 0:
                    resp = Message(
                        MessageType.RESPONSE,
                        {'客户端名称': self.name, '文本': "命令执行成功"},
                        sender=self.name,
                        target="server"
                    )
                    self.send_message(resp)
                else:
                    resp = Message(
                        MessageType.RESPONSE,
                        {'客户端名称': self.name, '文本': "命令执行失败"},
                        sender=self.name,
                        target="server"
                    )
                    self.send_message(resp)
                
        elif message.type == MessageType.BROADCAST:
            content = message.data.get('content', '')
            print(f"\n[广播] {message.sender}: {content}", flush=True)
            handle_msg(message.sender, content)
        elif message.type == MessageType.PRIVATE_MESSAGE:
            content = message.data.get('content', '')
            print(f"\n[私聊] {message.sender}: {content}", flush=True)

            if is_compile_command(content):
                resp = Message(
                    MessageType.RESPONSE,
                    {'客户端名称': self.name, '文本': "开始执行" + content},
                    sender=self.name,
                    target="server"
                )
                self.send_message(resp)
                
                ret = handle_msg(message.sender, content)
                if ret == 0:
                    resp = Message(
                        MessageType.RESPONSE,
                        {'客户端名称': self.name, '文本': "命令执行成功"},
                        sender=self.name,
                        target="server"
                    )
                    self.send_message(resp)
                else:
                    resp = Message(
                        MessageType.RESPONSE,
                        {'客户端名称': self.name, '文本': "命令执行失败"},
                        sender=self.name,
                        target="server"
                    )
                    self.send_message(resp)

    def connect(self, name: str) -> bool:
        # 先调用父类连接逻辑
        ok = super().connect(name)
        if ok:
            print(f"{name} 已连接，等待显示文本命令与编译指令...", flush=True)
        return ok

def main():
    """以命令行参数启动 Ubuntu 编译客户端。
    用法：python Client_Ubuntu.py <client_name> <tcp_url>
        client_name: 注册到服务端的名称，例如 ubuntu
        tcp_url:     服务器地址，例如 192.168.1.7 或 tcp://host
    """
    name = sys.argv[1] if len(sys.argv) >= 2 else "代码编译器"
    tcp_url = sys.argv[2] if len(sys.argv) >= 3 else "192.168.1.7"

    client = UbuntuTCPClient(tcp_url)
    if not client.connect(name):
        print("连接服务器失败")
        return 1

    try:
        while client.running:
            try:
                # Ubuntu 客户端无人值守，主要通过命令行交互输入回车退出
                line = sys.stdin.readline()
                if not line:
                    # 输入流关闭
                    break
                user_input = line.strip()
                if not user_input:
                    continue
                if user_input == '/quit':
                    break
                elif user_input == '/list':
                    client.get_clients()
                elif user_input.startswith('/msg '):
                    parts = user_input[5:].split(' ', 1)
                    if len(parts) == 2:
                        target, content = parts
                        client.send_private_message(target, content)
                    else:
                        print("格式错误: /msg <名称> <内容>")
                elif user_input.startswith('/broad '):
                    content = user_input[7:]
                    if content:
                        client.broadcast(content)
                    else:
                        print("格式错误: /broad <内容>")
                elif user_input == '/help':
                    print("可用命令:")
                    print("/list - 获取在线用户列表")
                    print("/msg <名称> <内容> - 发送私聊消息")
                    print("/broad <内容> - 广播消息")
                    print("/quit - 退出客户端")
                    print("等待服务下发显示文本命令/私聊中的编译指令：跑一下/编译/编译程序/重新编译/清理/打包")
                else:
                    # 直接输入的文本，也尝试当作自身接收到的指令来执行，便于本地调试
                    print(f"本地执行指令: {user_input}")
                    if is_compile_command(user_input):
                        resp = Message(
                            MessageType.RESPONSE,
                            {'客户端名称': client.name, '文本': "开始执行" + user_input},
                            sender=client.name,
                            target="server"
                        )
                        client.send_message(resp)
                        
                        ret = handle_msg("local", user_input)
                        if ret == 0:
                            resp = Message(
                                MessageType.RESPONSE,
                                {'客户端名称': client.name, '文本': "命令执行成功"},
                                sender=client.name,
                                target="server"
                            )
                            client.send_message(resp)
                        else:
                            resp = Message(
                                MessageType.RESPONSE,
                                {'客户端名称': client.name, '文本': "命令执行失败"},
                                sender=client.name,
                                target="server"
                            )
                            client.send_message(resp)
                        
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"处理输入时出错: {e}")
    finally:
        client.close()
        print("已断开连接")
    return 0

if __name__ == '__main__':
    main()
