struct s
{
	int a;
	int b;
};

int main()
{
	struct s var;

	var.a = 0;
	var.b = 2;

	return var.a + var.b;
}
