extern void print(int);
extern int read();

int func(int n){
	int max;
	int i;
	int a;
	max = 0;
	i = 0;

	while (i < n){
		a = read();
		if (a > max)
			max = a;
		i = i + 1;
	}

	return max;
}

