int main()
{
	int x;

	x = 1;
	while(x)
	{
		x = x - 1;
		if(x)
			x = x;
	}
	return x;
}