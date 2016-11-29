/* print.c
 * this file include a useful funcion print()
 * suppose %d %x %c %s now
 */

/* std */
#include <type.h>
#include <vga.h>
/* lib */
#include <string.h>
#include <print.h>

char* itoa( int value, char *str, int radix )
{
	char	reverse[36];
	char	*p	= reverse;
	bool	sign	= (value >= 0) ? TRUE : FALSE;

	value	= (value >= 0) ? value : -value;
	*p++	= '\0';
	while ( value >= 0 )
	{
		*p++	= "0123456789abcdef"[value % radix];
		value	/= radix;
		if ( value == 0 )
			break;
	}

	if ( !sign )
	{
		*p = '-';
	}else  {
		p--;
	}

	while ( p >= reverse )
	{
		*str++ = *p--;
	}

	return(str);
}


char* uitoa( uint32_t value, char *str, int radix )
{
	char	reverse[36];
	char	*p = reverse;

	*p++ = '\0';
	while ( value != 0 )
	{
		*p++	= "0123456789abcdef"[value % radix];
		value	/= radix;
		if ( value == 0 )
			break;
	}
	p--;

	while ( p >= reverse )
	{
		*str++ = *p--;
	}

	return(str);
}


void vsprint( char *buf, const char *fmt, va_list args )
{
	char	*p;
	va_list p_next_arg = args;

	for ( p = buf; *fmt; fmt++ )
	{
		if ( *fmt != '%' )
		{
			*p++ = *fmt;
			continue;
		}
		fmt++; /* *fmt = '%' */
		switch ( *fmt )
		{
		case 'd':
			itoa( va_arg( p_next_arg, int ), p, 10 );
			p += strlen( p );
			break;
		case 'x':
			uitoa( va_arg( p_next_arg, unsigned int ), p, 16 );
			p += strlen( p );
			break;
		case 'c':
			*p++ = va_arg( p_next_arg, char );
			break;
		case 's':
			*p = '\0';
			strcat( p, va_arg( p_next_arg, char * ) );
			p += strlen( p );
			break;
		default:
			break;
		}
	}
	*p = '\0';
}


void print( const char *fmt, ... )
{
	char	buf[256];
	va_list args;

	memset( buf, 0, sizeof(buf) );
	va_start( args, fmt );
	vsprint( buf, fmt, args );
	va_end( args );
	puts( buf );
}
