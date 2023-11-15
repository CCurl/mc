// test
{
	void ABC() {
		ABC=1123; u=ABC*ABC;
	}

	void DEF() {
		DEF=9999; y=DEF*DEF;
	}

	void main() { // the entry point
		ABC();
		a = 11; b=a+2; z=999999;
		if (a < 1) { c=a+b; }
		else { d = a*(b+1); }
		if (a > 1) { g=a*a; }
		e = b*7;
		f = e/3;
		i = 100*1000000;
		cnt=1;
		while (i) { i=i-1; cnt=cnt+1; }
		i=num=0;
		do { i=i+(num=num+1); } while (i<1000);
		DEF();
	}
}
