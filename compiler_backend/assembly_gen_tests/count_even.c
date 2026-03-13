extern void print(int);
extern int read();

int func(int n){
    int i;
    int a;
    int rem;
    int count;

    i = 0;
    count = 0;

    while (i < n){
        a = read();
        rem = a;

        while (rem > 1){
            rem = rem - 2;
        }

        if (rem < 1)
            count = count + 1;

        i = i + 1;
    }

    return count;
}