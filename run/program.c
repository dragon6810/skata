int global;

void func(int x, int y);

void func(int x, int y)
{
    int a;
    int b;
    int c;

    a = b = c = -10 * -4 - -2;
    a = b++;
    c = -++a;
}