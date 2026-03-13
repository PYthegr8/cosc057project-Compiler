extern void print(int);
extern int read();

//should return 14 if func(4)
int func(int n){
    int i;
    int sum;
    i = 1;
    sum = 0;

    while (i < n){
        sum = sum + i * i;
        i = i + 1;
    }

    return sum;
}