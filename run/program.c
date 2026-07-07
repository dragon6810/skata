struct s { int x; int y; };

int main()
{
	struct s a;
	struct s b;
	struct s c;
	struct s *ref;

	ref = &b;
	ref->x = 1;
	ref->y = 2;
	a = c = b;

	return a.x + a.y + c.x + c.y;
}