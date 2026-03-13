extern void print(int);
extern int read();

int func(int n){
    int min;
    int i;
    int a;

    min = read();
    i = 1;

    while (i < n){
        a = read();
        if (a < min)
            min = a;
        i = i + 1;
    }

    return min;
}