from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

out = Path('output/i2c')
out.mkdir(parents=True, exist_ok=True)
im = Image.new('RGB', (1480, 1580), '#ffffff')
d = ImageDraw.Draw(im)
font_path = 'C:/Windows/Fonts/msyh.ttc'
def font(size):
    return ImageFont.truetype(font_path, size)
blue, orange, red = '#2166ac', '#b65a12', '#c72532'
d.text((50, 28), 'I2C 基本信号：看跳变，也看时钟位置', font=font(40), fill='#17212b')
d.text((50, 91), '蓝色 SCL：时钟线    橙色 SDA：数据线    红点：起始 / 停止跳变', font=font(26), fill='#465565')

items = [
    ('① 总线空闲', [(0,1),(1,1)], [(0,1),(1,1)], None, None, 'STOP 后，两根线保持高电平'),
    ('② START 起始', [(0,1),(.8,1),(.8,0),(1,0)], [(0,1),(.5,1),(.5,0),(1,0)], .5, None, 'SCL 为高，SDA 从高变低'),
    ('③ Sr 重复起始', [(0,0),(.38,0),(.38,1),(.85,1),(.85,0),(1,0)], [(0,0),(.2,0),(.2,1),(.65,1),(.65,0),(1,0)], .65, None, '未先发 STOP，再发一次 START'),
    ('④ STOP 停止', [(0,0),(.25,0),(.25,1),(1,1)], [(0,0),(.65,0),(.65,1),(1,1)], .65, None, 'SCL 为高，SDA 从低变高'),
    ('⑤ 数据位 0', [(0,0),(.32,0),(.32,1),(.68,1),(.68,0),(1,0)], [(0,0),(1,0)], None, '第 1～8 个时钟中的一个', 'SCL 高电平期间，SDA 稳定为低'),
    ('⑥ 数据位 1', [(0,0),(.32,0),(.32,1),(.68,1),(.68,0),(1,0)], [(0,1),(1,1)], None, '第 1～8 个时钟中的一个', 'SCL 高电平期间，SDA 稳定为高'),
    ('⑦ ACK 应答', [(0,0),(.32,0),(.32,1),(.68,1),(.68,0),(1,0)], [(0,0),(1,0)], None, '第 9 个时钟', '发送方释放 SDA，接收方拉低 SDA'),
    ('⑧ NACK 非应答', [(0,0),(.32,0),(.32,1),(.68,1),(.68,0),(1,0)], [(0,1),(1,1)], None, '第 9 个时钟', '发送方释放 SDA，接收方也不拉低'),
]

for i, (title, scl, sda, event, phase, note) in enumerate(items):
    col, row = i % 2, i // 2
    ox, oy = 50 + col * 735, 155 + row * 345
    d.text((ox, oy), title, font=font(31), fill='#17212b')
    x0, x1 = ox + 105, ox + 635
    if phase:
        d.rectangle((x0 + .32*(x1-x0), oy+87, x0 + .68*(x1-x0), oy+235), fill='#eff4f8')
        d.text((ox+105, oy+51), phase, font=font(24), fill='#465565')
    for label, pts, hi, lo, color in [('SCL', scl, oy+105, oy+147, blue), ('SDA', sda, oy+190, oy+232, orange)]:
        d.text((ox, hi+4), label, font=font(27), fill=color)
        xy = [(x0+x*(x1-x0), hi if v else lo) for x,v in pts]
        d.line(xy, fill=color, width=5, joint='curve')
        d.text((x1+10, hi-12), '高', font=font(20), fill='#697987')
        d.text((x1+10, lo-12), '低', font=font(20), fill='#697987')
    if event is not None:
        ex, ey = x0+event*(x1-x0), oy+211
        d.line((ex, oy+96, ex, oy+244), fill='#e0a0a5', width=2)
        d.ellipse((ex-7, ey-7, ex+7, ey+7), fill=red)
    d.line((x0, oy+260, x1, oy+260), fill='#b7c2cc', width=2)
    d.polygon([(x1,oy+260),(x1-12,oy+254),(x1-12,oy+266)], fill='#b7c2cc')
    d.text((ox, oy+285), note, font=font(24), fill='#283847')

d.text((50, 1530), '时序示意，不按实际时间比例；每个小图分别表示一种情况。', font=font(25), fill='#465565')
im.save(out/'i2c-basic-signals.png')
print((out/'i2c-basic-signals.png').resolve())
