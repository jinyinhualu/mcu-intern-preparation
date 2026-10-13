from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

out=Path('output/i2c')
out.mkdir(parents=True,exist_ok=True)
im=Image.new('RGB',(1440,840),'white')
d=ImageDraw.Draw(im)
def f(n): return ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',n)
blue,orange='#2166ac','#b65a12'
d.text((45,25),'ACK / NACK 是应答；STOP 是另一个动作',font=f(38),fill='#17212b')
for row,title,pts,note in [
 (0,'主机写最后一个数据字节：从机 ACK → 主机 STOP',[(0,0),(.84,0),(.84,1),(1,1)],'ACK 确认收到；主机随后决定结束本次写操作'),
 (1,'主机读最后一个数据字节：主机 NACK → 主机 STOP',[(0,1),(.55,1),(.55,0),(.84,0),(.84,1),(1,1)],'NACK 后，主机先在 SCL 低时拉低 SDA，再产生 STOP')
]:
 oy=110+row*350
 d.text((45,oy),title,font=f(30),fill='#17212b')
 x0,x1=175,1340
 def x(t): return x0+t*(x1-x0)
 d.rectangle((x(.20),oy+94,x(.40),oy+252),fill='#eff4f8')
 d.text((x(.20)-18,oy+53),'第 9 个时钟',font=f(25),fill='#465565')
 scl=[(0,0),(.20,0),(.20,1),(.40,1),(.40,0),(.70,0),(.70,1),(1,1)]
 for label,signal,hi,lo,color in [('SCL',scl,oy+110,oy+155,blue),('SDA',pts,oy+205,oy+250,orange)]:
  d.text((45,hi+2),label,font=f(28),fill=color)
  d.line([(x(t),hi if v else lo) for t,v in signal],fill=color,width=5)
 ex=x(.84)
 d.line((ex,oy+99,ex,oy+265),fill='#d89a9f',width=2)
 d.ellipse((ex-7,oy+221,ex+7,oy+235),fill='#c72532')
 d.text((ex-25,oy+58),'STOP',font=f(26),fill='#c72532')
 d.text((45,oy+286),note,font=f(27),fill='#283847')
d.text((45,805),'时序示意，不按实际时间比例；仅画出最后一个字节之后的应答与停止。',font=f(23),fill='#465565')
im.save(out/'i2c-ack-nack-stop.png')
