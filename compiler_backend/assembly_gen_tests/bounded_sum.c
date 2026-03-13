extern void print(int);
extern int read();

int func(int n){
    int i;
    int a;
    int sum;

    i = 0;
    sum = 0;

    while (i < n){
        a = read();

        if (a > 5)
            a = 5;

        if (a < 2)
            a = 2;

        sum = sum + a;
        i = i + 1;
    }

    return sum;
}

/*
*Example input:1 8 3 10
*Expected output: In main printing return value of test: 15
 */