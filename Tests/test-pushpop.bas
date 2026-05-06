1 PRINT "start of push/pop test"
10 REM --- integer push/pop ---
20 PUSH 10
30 PUSH 20
40 PUSH 30
50 POP a
60 POP b
70 POP c
80 PRINT "int lifo: expect 30 20 10"
90 PRINT a
100 PRINT b
110 PRINT c
120 REM --- string push/pop ---
130 PUSH "alpha"
140 PUSH "beta"
150 PUSH "gamma"
160 POP a$
170 POP b$
180 POP c$
190 PRINT "str lifo: expect gamma beta alpha"
200 PRINT a$
210 PRINT b$
220 PRINT c$
230 REM --- expression push ---
240 LET x = 6
250 PUSH x * 7
260 POP d
270 PRINT "expr push: expect 42"
280 PRINT d
290 REM --- string expr push ---
300 LET s$ = "hel"
310 PUSH s$ + "lo"
320 POP e$
330 PRINT "str expr push: expect hello"
340 PRINT e$
350 REM --- type mismatch: int pushed, string popped ---
360 PUSH 99
370 POP f$
380 PRINT "should not reach here"
390 END
