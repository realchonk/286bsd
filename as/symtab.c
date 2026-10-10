unsigned
djbhash(s)
	char *s;
{
	unsigned hash = 5381;
	unsigned char c;

	while (c = *s++, c != 0)
		hash = 33 * hash + c;

	return (hash);
}
