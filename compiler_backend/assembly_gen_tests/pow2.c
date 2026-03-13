extern void print(int);
extern int read();

int func(int n){
    int i;
    int res;

    i = 0;
    res = 1;

    while (i < n){
        res = res * 2;
        i = i + 1;
    }

    return res;
}