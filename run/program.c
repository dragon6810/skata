int add(int a, int b)
{
	return a + b;
}

int main()
{
	int x;
	int y;

	x = 1;
	y = add(x, 0);
	x = add(x, y);

	return x;
}