
import random, urllib2, socket
import lxml.html as lh

player = None
target = None
remain = None

def news(term):
	#bbc news search
	url = "http://www.bbc.co.uk/search?q="+term+"&sa_f=search-product&filter=news"
	page = lh.parse(urllib2.urlopen(url))
	res = page.xpath('//li[@data-result-number="1"]/article/div/h1/a')
	if len(res) != 1:
		return "Not found"
	return "title: %s (%s)" % (res[0].text, res[0].attrib['href'])


def cal(x):
	'''
		don't accept negative operand, e.g. -2*1, 1*(-2)...
		don't accept unnecessary symbol, e.g. 1*+1
	'''
	post = []
	stack = []
	prio = {'(':0, '+':1, '-':1, '*':2, '/':2, '^':3}
	num = ""
	for c in x:
		if (c >= '0' and c <= '9') or c == '.':
			num+=c
		else: 
			if num != "":
				post.append(float(num))
				num = ""
			if c == '(':
				stack.append(c)
			elif c == ')':
				while 1:
					if len(stack) == 0:
						raise ValueError('Error, please check your input(())')
					d=stack.pop()
					if d == '(':
						break
					else:
						post.append(d)
			elif c == '+' or c == '-' or c == '*' or c == '/' or c == '^':
				while len(stack)>0 and prio[stack[-1]] >= prio[c]:
					post.append(stack.pop())
				stack.append(c)
			else:
				raise ValueError('Error, please check your input(@#a)')
	if num != "":
		post.append(float(num))
		num = ""		
	while len(stack) > 0:
		x = stack.pop()
		if x == '(':
			raise ValueError('Error, please check your input(())')
		post.append(x)
	
	#print post
	
	stack = []
	for p in post:
		if isinstance(p,float):
			stack.append(p)
		else:
			if len(stack) < 2:
				raise ValueError('Error, please check your input(+-*123)')
			b=stack.pop()
			a=stack.pop()
			if p == '+':
				stack.append(a+b)
			elif p == '-':
				stack.append(a-b)
			elif p == '*':
				stack.append(a*b)
			elif p == '/':
				stack.append(a/b)
			elif p == '^':
				stack.append(pow(a,b))
	if len(stack) != 1:
		raise ValueError('Error, please check your input(+-*123)')
	d = stack.pop()
	if d.is_integer():
		d = int(d)
	return d

def play(user):
	global player
	global target
	global remain
	if player != None:
		return None
	player = user
	target = random.randint(1,100)
	remain = 5
	return "Start! (1-100 with 5 times)"

def guess(user, num):
	global player
	global target
	global remain
	if player !=user:
		return None
	remain -= 1
	ret = None
	if remain == -1:
		ret = "Out of move (ans = %s)" % (target)
		player, target, remain = None, None, None
	elif num > target:
		ret = "Lower (%s)" % (remain)
	elif num < target:
		ret = "Higher (%s)" % (remain)
	else:
		ret = "Correct (%s)" % (remain)
		player, target, remain = None, None, None
	return ret

def help():
	return ["@repeat <string>",
			"@cal <expression>",
			"@play <robot name>",
			"@guess <integer>",
			"@news <keyword>"]
	
if __name__ == '__main__':
	f = open('config','r')
	channel = f.readline()[6:-2]
	key = f.readline()[10:-2]
	f.close()
	myname = 'hi_bot'
	IRCSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	IRCSocket.connect(("irc.freenode.net", 6667))
	
	IRCSocket.send(bytes("NICK %s\r\n" % myname))
	IRCSocket.send(bytes("USER %s hi1 hi2 :hi hi\r\n" % myname))
	IRCSocket.send(bytes("JOIN %s %s\r\n" % (channel, key)))
	
	IRCSocket.send(bytes("PRIVMSG %s :Hi, I'm %s\r\n" % (channel, myname)))
	
	while True:
		IRCMsg = IRCSocket.recv(4096)
		print IRCMsg
		
		if IRCMsg[0:4] == "PING":
			IRCSocket.send(bytes("PONG %s \r\n" % (IRCMsg.split()[1])))
			continue
		MsgArr = IRCMsg.split(' ',3)
		MsgArr = filter(None, MsgArr)
		
		if len(MsgArr) > 3 and MsgArr[1] == "PRIVMSG":
			user = MsgArr[0].split('!',1)[0][1:]
			cmd, content = None, None
			MsgArr[3] = MsgArr[3][1:]
			MsgArr[3] = MsgArr[3].strip()
			print MsgArr[3]
			if len(MsgArr[3].split(' ')) < 2:
				cmd = MsgArr[3]
				cmd = cmd.strip()
			else:
				cmd, content = MsgArr[3].split(' ',1)
				content = content.strip()
			print cmd
			print content
			if cmd[0]!='@':
				continue
				
			print user, cmd, content #debug
			
			if cmd == "@repeat":
				IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, content, user)))
			elif cmd == "@cal":
				try:
					res = cal("".join(content.split()))
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, str(res), user)))
				except ValueError as e:
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, str(e), user)))
			elif cmd == "@play" and content == myname:
				res = play(user)
				if res == None:
					continue
				IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, res, user)))
			elif cmd == "@guess":
				try:
					res = guess(user, int(content))
					if res == None:
						continue
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, res, user)))
				except ValueError as e:
					err = "Error, guess with Integer"
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, err, user)))
			elif cmd == "@news":
				try:
					res = news(content)
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, res, user)))
				except:
					err = "Oops, something wrong"
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, err, user)))
			elif cmd == "@help":
				res = help()
				for r in res:
					IRCSocket.send(bytes("PRIVMSG %s :%s (%s)\r\n" % (channel, r, user)))
				
		
