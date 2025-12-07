int add(int a, int b)
{
	return a + b;
}

int main()
{
	int x;
	int y;
	int z;

	x = 1;
	y = add(x, 0);
	x = add(x, y);

	if(x)
	{
		z = add(x, y);
		x = add(y, z);
	}

	return x;
}