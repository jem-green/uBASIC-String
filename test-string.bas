1 PRINT "start of test"
2 LET a$= "abcdefghi"
3 b$="123456789"
5 PRINT "Length of a$=", LEN(a$)
6 PRINT "Length of b$=", LEN(b$)
7 IF LEN(a$) = LEN(b$) THEN PRINT "same length"
8 IF (a$ = b$) THEN PRINT "same string"
9 c$=LEFT$(a$+ b$,12)
10 PRINT c$
11 c$=RIGHT$(a$+b$, 12)
12 PRINT c$
13 c$=MID$(a$+b$, 8,8)
14 PRINT c$
15 c$=STR$(13+42)
16 PRINT c$
17 PRINT LEN(c$)
18 PRINT LEN("this" + "that")
19 c$ = CHR$(34)
20 PRINT c$
21 j = ASC(c$)
22 PRINT j
23 PRINT VAL("12345")
24 i=INSTR(3, "123456789", "67")
24 PRINT "position of '67' in '123456789' is", i
25 PRINT MID$(a$,2,2)+"xyx"
30 PRINT "end of test"
40 END
