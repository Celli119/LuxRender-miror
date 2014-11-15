/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

// Based on the public domain reference implementation 
// http://www.cs.technion.ac.il/~biham/Reports/Tiger/

#include "tigerhash.h"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>
#include "osfunc.h"

using namespace lux;

using std::memmove;

typedef uint64_t word64;
typedef uint32_t word32;
typedef uint8_t byte;


// The number of passes of the hash function.
// Three passes are recommended.
// Use four passes when you need extra security.
// Must be at least three.
const unsigned int tiger_passes = 3;

#define tiger_t1 (table)
#define tiger_t2 (table+256)
#define tiger_t3 (table+256*2)
#define tiger_t4 (table+256*3)

/* This is the official definition of round */
#define tiger_round(a,b,c,x,mul) \
      c ^= x; \
      a -= tiger_t1[((c)>>(0*8))&0xFF] ^ tiger_t2[((c)>>(2*8))&0xFF] ^ \
	       tiger_t3[((c)>>(4*8))&0xFF] ^ tiger_t4[((c)>>(6*8))&0xFF] ; \
      b += tiger_t4[((c)>>(1*8))&0xFF] ^ tiger_t3[((c)>>(3*8))&0xFF] ^ \
	       tiger_t2[((c)>>(5*8))&0xFF] ^ tiger_t1[((c)>>(7*8))&0xFF] ; \
      b *= mul;

#define tiger_pass(a,b,c,mul) \
      tiger_round(a,b,c,x0,mul) \
      tiger_round(b,c,a,x1,mul) \
      tiger_round(c,a,b,x2,mul) \
      tiger_round(a,b,c,x3,mul) \
      tiger_round(b,c,a,x4,mul) \
      tiger_round(c,a,b,x5,mul) \
      tiger_round(a,b,c,x6,mul) \
      tiger_round(b,c,a,x7,mul)

static void key_schedule(word64 &x0, word64 &x1, word64 &x2, word64 &x3, word64 &x4, word64 &x5, word64 &x6, word64 &x7) {
      x0 -= x7 ^ 0xA5A5A5A5A5A5A5A5LL;
      x1 ^= x0;
      x2 += x1;
      x3 -= x2 ^ ((~x1)<<19);
      x4 ^= x3;
      x5 += x4;
      x6 -= x5 ^ ((~x4)>>23);
      x7 ^= x6;
      x0 += x7;
      x1 -= x0 ^ ((~x7)<<19);
      x2 ^= x1;
      x3 += x2;
      x4 -= x3 ^ ((~x2)>>23);
      x5 ^= x4;
      x6 += x5;
      x7 -= x6 ^ 0x0123456789ABCDEFLL;
}

/* Encapsulates the compress function. */
void tigerhash::compress(const uint8_t *block, uint64_t state[3]) const {
	const uint64_t *str = reinterpret_cast<const uint64_t*>(block);

	word64 a, b, c; 
	word64 x0, x1, x2, x3, x4, x5, x6, x7;

	a = state[0];
	b = state[1];
	c = state[2];

	x0=str[0]; x1=str[1]; x2=str[2]; x3=str[3];
	x4=str[4]; x5=str[5]; x6=str[6]; x7=str[7];

	// save abc
    const word64 aa = a;
    const word64 bb = b;
    const word64 cc = c;
	// compress
    for(unsigned int pass_no = 0; pass_no < tiger_passes; pass_no++) {
		if (pass_no != 0) {
			key_schedule(x0, x1, x2, x3, x4, x5, x6, x7);
		}
		tiger_pass(a, b, c, (pass_no == 0 ? 5 : (pass_no == 1 ? 7 : 9)));
		word64 tmpa = a;
		a = c; 
		c = b; 
		b = tmpa;
	}
    // feedforward
    a ^= aa;
    b -= bb;
    c += cc;

	state[0] = a;
	state[1] = b;
	state[2] = c;
}

tigerhash::tigerhash() {
	restart();
}

tigerhash::tigerhash(tigerhash &other){
	count = other.count;
	memmove(buffer, other.buffer, sizeof(buffer));
	memmove(res, other.res, sizeof(res));
}

tigerhash::digest_type tigerhash::end_message() const {
	tigerhash::digest_type hash;
	memmove(&hash[0], res, sizeof(res));

	word64* h = reinterpret_cast<word64*>(&hash[0]);

	byte temp[64];
	memmove(temp, buffer, sizeof(temp));

	word64 j = count & 63;
	if (!osIsLittleEndian()) {
		temp[j^7] = 0x01;
		j++;
		for(; j & 7; j++)
			temp[j^7] = 0;
	} else {
		temp[j++] = 0x01;
		for(; j & 7; j++)
			temp[j] = 0;
	}

	if (j > 56) {
		for (; j < 64; j++)
			temp[j] = 0;
		compress(temp, h);
		j = 0;
    }

	for(; j < 56; j++)
		temp[j] = 0;

	// TODO - probably not endian safe
	//*(reinterpret_cast<word64*>(&temp[56])) = count << 3;
	const word64 bitcount = count << 3;
	memmove(&temp[56], &bitcount, 8);

	compress(temp, h);

	return hash;
}

void tigerhash::restart() {
	count = 0;
	memset(buffer, 0, sizeof(buffer));
	res[0] = 0x0123456789ABCDEFULL;
	res[1] = 0xFEDCBA9876543210ULL;
	res[2] = 0xF096A5B4C3B2E187ULL;
}

void tigerhash::update(const char* data, uint64_t length) {
	byte temp[64]; // for big endian mode
	word64 i = length;

	// fill buffer in case we didnt get a full 64byte block last update
	unsigned int r = static_cast<unsigned int>(count & 63);

	// update count so it's done
	count += length;

	if (r > 0) {
		unsigned int rc = static_cast<unsigned int>(std::min(static_cast<word64>(64 - r), i));
		for (unsigned int k = rc; k > 0; k--)
		{
			buffer[r++] = *data;
			data++;
			i--;
			if (i == 0)
				return;
		}
		compress(buffer, res);
		//memset(buffer, 0, sizeof(buffer));
	}

	for (; i >= 64; i -= 64)
    {
		if (!osIsLittleEndian()) {
			for(unsigned int k = 0; k < 64; k++)
				temp[k^7] = data[k];
			compress(temp, res);
		} else {
			compress(reinterpret_cast<const uint8_t*>(data), res);
		}
		data += 64;
    }

	if (!osIsLittleEndian()) {
		for (unsigned int j = 0; j < static_cast<unsigned int>(i); j++)
			buffer[j^7] = data[j];
	} else {
		for (unsigned int j = 0; j < static_cast<unsigned int>(i); j++)
			buffer[j] = data[j];
	}
}

std::string lux::digest_string(const tigerhash::digest_type &d) {
	std::ostringstream ss("");
	ss << d;
	return ss.str();
}

std::ostream &operator<<(std::ostream &os, const tigerhash::digest_type &d) {
	const char hex_chars[] = "0123456789abcdef";

	std::string s(d.size()*2, 0);

	for (size_t i = 0; i < d.size(); i++) {
		s[2*i+0] = hex_chars[(d[d.size()-1-i] >> 4) & 15];
		s[2*i+1] = hex_chars[(d[d.size()-1-i] >> 0) & 15];
	}

	os << s;

	return os;
}




/* sboxes.c: Tiger S boxes */
uint64_t tigerhash::table[4*256] = {
    0x02AAB17CF7E90C5EULL   /*    0 */,    0xAC424B03E243A8ECULL   /*    1 */,
    0x72CD5BE30DD5FCD3ULL   /*    2 */,    0x6D019B93F6F97F3AULL   /*    3 */,
    0xCD9978FFD21F9193ULL   /*    4 */,    0x7573A1C9708029E2ULL   /*    5 */,
    0xB164326B922A83C3ULL   /*    6 */,    0x46883EEE04915870ULL   /*    7 */,
    0xEAACE3057103ECE6ULL   /*    8 */,    0xC54169B808A3535CULL   /*    9 */,
    0x4CE754918DDEC47CULL   /*   10 */,    0x0AA2F4DFDC0DF40CULL   /*   11 */,
    0x10B76F18A74DBEFAULL   /*   12 */,    0xC6CCB6235AD1AB6AULL   /*   13 */,
    0x13726121572FE2FFULL   /*   14 */,    0x1A488C6F199D921EULL   /*   15 */,
    0x4BC9F9F4DA0007CAULL   /*   16 */,    0x26F5E6F6E85241C7ULL   /*   17 */,
    0x859079DBEA5947B6ULL   /*   18 */,    0x4F1885C5C99E8C92ULL   /*   19 */,
    0xD78E761EA96F864BULL   /*   20 */,    0x8E36428C52B5C17DULL   /*   21 */,
    0x69CF6827373063C1ULL   /*   22 */,    0xB607C93D9BB4C56EULL   /*   23 */,
    0x7D820E760E76B5EAULL   /*   24 */,    0x645C9CC6F07FDC42ULL   /*   25 */,
    0xBF38A078243342E0ULL   /*   26 */,    0x5F6B343C9D2E7D04ULL   /*   27 */,
    0xF2C28AEB600B0EC6ULL   /*   28 */,    0x6C0ED85F7254BCACULL   /*   29 */,
    0x71592281A4DB4FE5ULL   /*   30 */,    0x1967FA69CE0FED9FULL   /*   31 */,
    0xFD5293F8B96545DBULL   /*   32 */,    0xC879E9D7F2A7600BULL   /*   33 */,
    0x860248920193194EULL   /*   34 */,    0xA4F9533B2D9CC0B3ULL   /*   35 */,
    0x9053836C15957613ULL   /*   36 */,    0xDB6DCF8AFC357BF1ULL   /*   37 */,
    0x18BEEA7A7A370F57ULL   /*   38 */,    0x037117CA50B99066ULL   /*   39 */,
    0x6AB30A9774424A35ULL   /*   40 */,    0xF4E92F02E325249BULL   /*   41 */,
    0x7739DB07061CCAE1ULL   /*   42 */,    0xD8F3B49CECA42A05ULL   /*   43 */,
    0xBD56BE3F51382F73ULL   /*   44 */,    0x45FAED5843B0BB28ULL   /*   45 */,
    0x1C813D5C11BF1F83ULL   /*   46 */,    0x8AF0E4B6D75FA169ULL   /*   47 */,
    0x33EE18A487AD9999ULL   /*   48 */,    0x3C26E8EAB1C94410ULL   /*   49 */,
    0xB510102BC0A822F9ULL   /*   50 */,    0x141EEF310CE6123BULL   /*   51 */,
    0xFC65B90059DDB154ULL   /*   52 */,    0xE0158640C5E0E607ULL   /*   53 */,
    0x884E079826C3A3CFULL   /*   54 */,    0x930D0D9523C535FDULL   /*   55 */,
    0x35638D754E9A2B00ULL   /*   56 */,    0x4085FCCF40469DD5ULL   /*   57 */,
    0xC4B17AD28BE23A4CULL   /*   58 */,    0xCAB2F0FC6A3E6A2EULL   /*   59 */,
    0x2860971A6B943FCDULL   /*   60 */,    0x3DDE6EE212E30446ULL   /*   61 */,
    0x6222F32AE01765AEULL   /*   62 */,    0x5D550BB5478308FEULL   /*   63 */,
    0xA9EFA98DA0EDA22AULL   /*   64 */,    0xC351A71686C40DA7ULL   /*   65 */,
    0x1105586D9C867C84ULL   /*   66 */,    0xDCFFEE85FDA22853ULL   /*   67 */,
    0xCCFBD0262C5EEF76ULL   /*   68 */,    0xBAF294CB8990D201ULL   /*   69 */,
    0xE69464F52AFAD975ULL   /*   70 */,    0x94B013AFDF133E14ULL   /*   71 */,
    0x06A7D1A32823C958ULL   /*   72 */,    0x6F95FE5130F61119ULL   /*   73 */,
    0xD92AB34E462C06C0ULL   /*   74 */,    0xED7BDE33887C71D2ULL   /*   75 */,
    0x79746D6E6518393EULL   /*   76 */,    0x5BA419385D713329ULL   /*   77 */,
    0x7C1BA6B948A97564ULL   /*   78 */,    0x31987C197BFDAC67ULL   /*   79 */,
    0xDE6C23C44B053D02ULL   /*   80 */,    0x581C49FED002D64DULL   /*   81 */,
    0xDD474D6338261571ULL   /*   82 */,    0xAA4546C3E473D062ULL   /*   83 */,
    0x928FCE349455F860ULL   /*   84 */,    0x48161BBACAAB94D9ULL   /*   85 */,
    0x63912430770E6F68ULL   /*   86 */,    0x6EC8A5E602C6641CULL   /*   87 */,
    0x87282515337DDD2BULL   /*   88 */,    0x2CDA6B42034B701BULL   /*   89 */,
    0xB03D37C181CB096DULL   /*   90 */,    0xE108438266C71C6FULL   /*   91 */,
    0x2B3180C7EB51B255ULL   /*   92 */,    0xDF92B82F96C08BBCULL   /*   93 */,
    0x5C68C8C0A632F3BAULL   /*   94 */,    0x5504CC861C3D0556ULL   /*   95 */,
    0xABBFA4E55FB26B8FULL   /*   96 */,    0x41848B0AB3BACEB4ULL   /*   97 */,
    0xB334A273AA445D32ULL   /*   98 */,    0xBCA696F0A85AD881ULL   /*   99 */,
    0x24F6EC65B528D56CULL   /*  100 */,    0x0CE1512E90F4524AULL   /*  101 */,
    0x4E9DD79D5506D35AULL   /*  102 */,    0x258905FAC6CE9779ULL   /*  103 */,
    0x2019295B3E109B33ULL   /*  104 */,    0xF8A9478B73A054CCULL   /*  105 */,
    0x2924F2F934417EB0ULL   /*  106 */,    0x3993357D536D1BC4ULL   /*  107 */,
    0x38A81AC21DB6FF8BULL   /*  108 */,    0x47C4FBF17D6016BFULL   /*  109 */,
    0x1E0FAADD7667E3F5ULL   /*  110 */,    0x7ABCFF62938BEB96ULL   /*  111 */,
    0xA78DAD948FC179C9ULL   /*  112 */,    0x8F1F98B72911E50DULL   /*  113 */,
    0x61E48EAE27121A91ULL   /*  114 */,    0x4D62F7AD31859808ULL   /*  115 */,
    0xECEBA345EF5CEAEBULL   /*  116 */,    0xF5CEB25EBC9684CEULL   /*  117 */,
    0xF633E20CB7F76221ULL   /*  118 */,    0xA32CDF06AB8293E4ULL   /*  119 */,
    0x985A202CA5EE2CA4ULL   /*  120 */,    0xCF0B8447CC8A8FB1ULL   /*  121 */,
    0x9F765244979859A3ULL   /*  122 */,    0xA8D516B1A1240017ULL   /*  123 */,
    0x0BD7BA3EBB5DC726ULL   /*  124 */,    0xE54BCA55B86ADB39ULL   /*  125 */,
    0x1D7A3AFD6C478063ULL   /*  126 */,    0x519EC608E7669EDDULL   /*  127 */,
    0x0E5715A2D149AA23ULL   /*  128 */,    0x177D4571848FF194ULL   /*  129 */,
    0xEEB55F3241014C22ULL   /*  130 */,    0x0F5E5CA13A6E2EC2ULL   /*  131 */,
    0x8029927B75F5C361ULL   /*  132 */,    0xAD139FABC3D6E436ULL   /*  133 */,
    0x0D5DF1A94CCF402FULL   /*  134 */,    0x3E8BD948BEA5DFC8ULL   /*  135 */,
    0xA5A0D357BD3FF77EULL   /*  136 */,    0xA2D12E251F74F645ULL   /*  137 */,
    0x66FD9E525E81A082ULL   /*  138 */,    0x2E0C90CE7F687A49ULL   /*  139 */,
    0xC2E8BCBEBA973BC5ULL   /*  140 */,    0x000001BCE509745FULL   /*  141 */,
    0x423777BBE6DAB3D6ULL   /*  142 */,    0xD1661C7EAEF06EB5ULL   /*  143 */,
    0xA1781F354DAACFD8ULL   /*  144 */,    0x2D11284A2B16AFFCULL   /*  145 */,
    0xF1FC4F67FA891D1FULL   /*  146 */,    0x73ECC25DCB920ADAULL   /*  147 */,
    0xAE610C22C2A12651ULL   /*  148 */,    0x96E0A810D356B78AULL   /*  149 */,
    0x5A9A381F2FE7870FULL   /*  150 */,    0xD5AD62EDE94E5530ULL   /*  151 */,
    0xD225E5E8368D1427ULL   /*  152 */,    0x65977B70C7AF4631ULL   /*  153 */,
    0x99F889B2DE39D74FULL   /*  154 */,    0x233F30BF54E1D143ULL   /*  155 */,
    0x9A9675D3D9A63C97ULL   /*  156 */,    0x5470554FF334F9A8ULL   /*  157 */,
    0x166ACB744A4F5688ULL   /*  158 */,    0x70C74CAAB2E4AEADULL   /*  159 */,
    0xF0D091646F294D12ULL   /*  160 */,    0x57B82A89684031D1ULL   /*  161 */,
    0xEFD95A5A61BE0B6BULL   /*  162 */,    0x2FBD12E969F2F29AULL   /*  163 */,
    0x9BD37013FEFF9FE8ULL   /*  164 */,    0x3F9B0404D6085A06ULL   /*  165 */,
    0x4940C1F3166CFE15ULL   /*  166 */,    0x09542C4DCDF3DEFBULL   /*  167 */,
    0xB4C5218385CD5CE3ULL   /*  168 */,    0xC935B7DC4462A641ULL   /*  169 */,
    0x3417F8A68ED3B63FULL   /*  170 */,    0xB80959295B215B40ULL   /*  171 */,
    0xF99CDAEF3B8C8572ULL   /*  172 */,    0x018C0614F8FCB95DULL   /*  173 */,
    0x1B14ACCD1A3ACDF3ULL   /*  174 */,    0x84D471F200BB732DULL   /*  175 */,
    0xC1A3110E95E8DA16ULL   /*  176 */,    0x430A7220BF1A82B8ULL   /*  177 */,
    0xB77E090D39DF210EULL   /*  178 */,    0x5EF4BD9F3CD05E9DULL   /*  179 */,
    0x9D4FF6DA7E57A444ULL   /*  180 */,    0xDA1D60E183D4A5F8ULL   /*  181 */,
    0xB287C38417998E47ULL   /*  182 */,    0xFE3EDC121BB31886ULL   /*  183 */,
    0xC7FE3CCC980CCBEFULL   /*  184 */,    0xE46FB590189BFD03ULL   /*  185 */,
    0x3732FD469A4C57DCULL   /*  186 */,    0x7EF700A07CF1AD65ULL   /*  187 */,
    0x59C64468A31D8859ULL   /*  188 */,    0x762FB0B4D45B61F6ULL   /*  189 */,
    0x155BAED099047718ULL   /*  190 */,    0x68755E4C3D50BAA6ULL   /*  191 */,
    0xE9214E7F22D8B4DFULL   /*  192 */,    0x2ADDBF532EAC95F4ULL   /*  193 */,
    0x32AE3909B4BD0109ULL   /*  194 */,    0x834DF537B08E3450ULL   /*  195 */,
    0xFA209DA84220728DULL   /*  196 */,    0x9E691D9B9EFE23F7ULL   /*  197 */,
    0x0446D288C4AE8D7FULL   /*  198 */,    0x7B4CC524E169785BULL   /*  199 */,
    0x21D87F0135CA1385ULL   /*  200 */,    0xCEBB400F137B8AA5ULL   /*  201 */,
    0x272E2B66580796BEULL   /*  202 */,    0x3612264125C2B0DEULL   /*  203 */,
    0x057702BDAD1EFBB2ULL   /*  204 */,    0xD4BABB8EACF84BE9ULL   /*  205 */,
    0x91583139641BC67BULL   /*  206 */,    0x8BDC2DE08036E024ULL   /*  207 */,
    0x603C8156F49F68EDULL   /*  208 */,    0xF7D236F7DBEF5111ULL   /*  209 */,
    0x9727C4598AD21E80ULL   /*  210 */,    0xA08A0896670A5FD7ULL   /*  211 */,
    0xCB4A8F4309EBA9CBULL   /*  212 */,    0x81AF564B0F7036A1ULL   /*  213 */,
    0xC0B99AA778199ABDULL   /*  214 */,    0x959F1EC83FC8E952ULL   /*  215 */,
    0x8C505077794A81B9ULL   /*  216 */,    0x3ACAAF8F056338F0ULL   /*  217 */,
    0x07B43F50627A6778ULL   /*  218 */,    0x4A44AB49F5ECCC77ULL   /*  219 */,
    0x3BC3D6E4B679EE98ULL   /*  220 */,    0x9CC0D4D1CF14108CULL   /*  221 */,
    0x4406C00B206BC8A0ULL   /*  222 */,    0x82A18854C8D72D89ULL   /*  223 */,
    0x67E366B35C3C432CULL   /*  224 */,    0xB923DD61102B37F2ULL   /*  225 */,
    0x56AB2779D884271DULL   /*  226 */,    0xBE83E1B0FF1525AFULL   /*  227 */,
    0xFB7C65D4217E49A9ULL   /*  228 */,    0x6BDBE0E76D48E7D4ULL   /*  229 */,
    0x08DF828745D9179EULL   /*  230 */,    0x22EA6A9ADD53BD34ULL   /*  231 */,
    0xE36E141C5622200AULL   /*  232 */,    0x7F805D1B8CB750EEULL   /*  233 */,
    0xAFE5C7A59F58E837ULL   /*  234 */,    0xE27F996A4FB1C23CULL   /*  235 */,
    0xD3867DFB0775F0D0ULL   /*  236 */,    0xD0E673DE6E88891AULL   /*  237 */,
    0x123AEB9EAFB86C25ULL   /*  238 */,    0x30F1D5D5C145B895ULL   /*  239 */,
    0xBB434A2DEE7269E7ULL   /*  240 */,    0x78CB67ECF931FA38ULL   /*  241 */,
    0xF33B0372323BBF9CULL   /*  242 */,    0x52D66336FB279C74ULL   /*  243 */,
    0x505F33AC0AFB4EAAULL   /*  244 */,    0xE8A5CD99A2CCE187ULL   /*  245 */,
    0x534974801E2D30BBULL   /*  246 */,    0x8D2D5711D5876D90ULL   /*  247 */,
    0x1F1A412891BC038EULL   /*  248 */,    0xD6E2E71D82E56648ULL   /*  249 */,
    0x74036C3A497732B7ULL   /*  250 */,    0x89B67ED96361F5ABULL   /*  251 */,
    0xFFED95D8F1EA02A2ULL   /*  252 */,    0xE72B3BD61464D43DULL   /*  253 */,
    0xA6300F170BDC4820ULL   /*  254 */,    0xEBC18760ED78A77AULL   /*  255 */,
    0xE6A6BE5A05A12138ULL   /*  256 */,    0xB5A122A5B4F87C98ULL   /*  257 */,
    0x563C6089140B6990ULL   /*  258 */,    0x4C46CB2E391F5DD5ULL   /*  259 */,
    0xD932ADDBC9B79434ULL   /*  260 */,    0x08EA70E42015AFF5ULL   /*  261 */,
    0xD765A6673E478CF1ULL   /*  262 */,    0xC4FB757EAB278D99ULL   /*  263 */,
    0xDF11C6862D6E0692ULL   /*  264 */,    0xDDEB84F10D7F3B16ULL   /*  265 */,
    0x6F2EF604A665EA04ULL   /*  266 */,    0x4A8E0F0FF0E0DFB3ULL   /*  267 */,
    0xA5EDEEF83DBCBA51ULL   /*  268 */,    0xFC4F0A2A0EA4371EULL   /*  269 */,
    0xE83E1DA85CB38429ULL   /*  270 */,    0xDC8FF882BA1B1CE2ULL   /*  271 */,
    0xCD45505E8353E80DULL   /*  272 */,    0x18D19A00D4DB0717ULL   /*  273 */,
    0x34A0CFEDA5F38101ULL   /*  274 */,    0x0BE77E518887CAF2ULL   /*  275 */,
    0x1E341438B3C45136ULL   /*  276 */,    0xE05797F49089CCF9ULL   /*  277 */,
    0xFFD23F9DF2591D14ULL   /*  278 */,    0x543DDA228595C5CDULL   /*  279 */,
    0x661F81FD99052A33ULL   /*  280 */,    0x8736E641DB0F7B76ULL   /*  281 */,
    0x15227725418E5307ULL   /*  282 */,    0xE25F7F46162EB2FAULL   /*  283 */,
    0x48A8B2126C13D9FEULL   /*  284 */,    0xAFDC541792E76EEAULL   /*  285 */,
    0x03D912BFC6D1898FULL   /*  286 */,    0x31B1AAFA1B83F51BULL   /*  287 */,
    0xF1AC2796E42AB7D9ULL   /*  288 */,    0x40A3A7D7FCD2EBACULL   /*  289 */,
    0x1056136D0AFBBCC5ULL   /*  290 */,    0x7889E1DD9A6D0C85ULL   /*  291 */,
    0xD33525782A7974AAULL   /*  292 */,    0xA7E25D09078AC09BULL   /*  293 */,
    0xBD4138B3EAC6EDD0ULL   /*  294 */,    0x920ABFBE71EB9E70ULL   /*  295 */,
    0xA2A5D0F54FC2625CULL   /*  296 */,    0xC054E36B0B1290A3ULL   /*  297 */,
    0xF6DD59FF62FE932BULL   /*  298 */,    0x3537354511A8AC7DULL   /*  299 */,
    0xCA845E9172FADCD4ULL   /*  300 */,    0x84F82B60329D20DCULL   /*  301 */,
    0x79C62CE1CD672F18ULL   /*  302 */,    0x8B09A2ADD124642CULL   /*  303 */,
    0xD0C1E96A19D9E726ULL   /*  304 */,    0x5A786A9B4BA9500CULL   /*  305 */,
    0x0E020336634C43F3ULL   /*  306 */,    0xC17B474AEB66D822ULL   /*  307 */,
    0x6A731AE3EC9BAAC2ULL   /*  308 */,    0x8226667AE0840258ULL   /*  309 */,
    0x67D4567691CAECA5ULL   /*  310 */,    0x1D94155C4875ADB5ULL   /*  311 */,
    0x6D00FD985B813FDFULL   /*  312 */,    0x51286EFCB774CD06ULL   /*  313 */,
    0x5E8834471FA744AFULL   /*  314 */,    0xF72CA0AEE761AE2EULL   /*  315 */,
    0xBE40E4CDAEE8E09AULL   /*  316 */,    0xE9970BBB5118F665ULL   /*  317 */,
    0x726E4BEB33DF1964ULL   /*  318 */,    0x703B000729199762ULL   /*  319 */,
    0x4631D816F5EF30A7ULL   /*  320 */,    0xB880B5B51504A6BEULL   /*  321 */,
    0x641793C37ED84B6CULL   /*  322 */,    0x7B21ED77F6E97D96ULL   /*  323 */,
    0x776306312EF96B73ULL   /*  324 */,    0xAE528948E86FF3F4ULL   /*  325 */,
    0x53DBD7F286A3F8F8ULL   /*  326 */,    0x16CADCE74CFC1063ULL   /*  327 */,
    0x005C19BDFA52C6DDULL   /*  328 */,    0x68868F5D64D46AD3ULL   /*  329 */,
    0x3A9D512CCF1E186AULL   /*  330 */,    0x367E62C2385660AEULL   /*  331 */,
    0xE359E7EA77DCB1D7ULL   /*  332 */,    0x526C0773749ABE6EULL   /*  333 */,
    0x735AE5F9D09F734BULL   /*  334 */,    0x493FC7CC8A558BA8ULL   /*  335 */,
    0xB0B9C1533041AB45ULL   /*  336 */,    0x321958BA470A59BDULL   /*  337 */,
    0x852DB00B5F46C393ULL   /*  338 */,    0x91209B2BD336B0E5ULL   /*  339 */,
    0x6E604F7D659EF19FULL   /*  340 */,    0xB99A8AE2782CCB24ULL   /*  341 */,
    0xCCF52AB6C814C4C7ULL   /*  342 */,    0x4727D9AFBE11727BULL   /*  343 */,
    0x7E950D0C0121B34DULL   /*  344 */,    0x756F435670AD471FULL   /*  345 */,
    0xF5ADD442615A6849ULL   /*  346 */,    0x4E87E09980B9957AULL   /*  347 */,
    0x2ACFA1DF50AEE355ULL   /*  348 */,    0xD898263AFD2FD556ULL   /*  349 */,
    0xC8F4924DD80C8FD6ULL   /*  350 */,    0xCF99CA3D754A173AULL   /*  351 */,
    0xFE477BACAF91BF3CULL   /*  352 */,    0xED5371F6D690C12DULL   /*  353 */,
    0x831A5C285E687094ULL   /*  354 */,    0xC5D3C90A3708A0A4ULL   /*  355 */,
    0x0F7F903717D06580ULL   /*  356 */,    0x19F9BB13B8FDF27FULL   /*  357 */,
    0xB1BD6F1B4D502843ULL   /*  358 */,    0x1C761BA38FFF4012ULL   /*  359 */,
    0x0D1530C4E2E21F3BULL   /*  360 */,    0x8943CE69A7372C8AULL   /*  361 */,
    0xE5184E11FEB5CE66ULL   /*  362 */,    0x618BDB80BD736621ULL   /*  363 */,
    0x7D29BAD68B574D0BULL   /*  364 */,    0x81BB613E25E6FE5BULL   /*  365 */,
    0x071C9C10BC07913FULL   /*  366 */,    0xC7BEEB7909AC2D97ULL   /*  367 */,
    0xC3E58D353BC5D757ULL   /*  368 */,    0xEB017892F38F61E8ULL   /*  369 */,
    0xD4EFFB9C9B1CC21AULL   /*  370 */,    0x99727D26F494F7ABULL   /*  371 */,
    0xA3E063A2956B3E03ULL   /*  372 */,    0x9D4A8B9A4AA09C30ULL   /*  373 */,
    0x3F6AB7D500090FB4ULL   /*  374 */,    0x9CC0F2A057268AC0ULL   /*  375 */,
    0x3DEE9D2DEDBF42D1ULL   /*  376 */,    0x330F49C87960A972ULL   /*  377 */,
    0xC6B2720287421B41ULL   /*  378 */,    0x0AC59EC07C00369CULL   /*  379 */,
    0xEF4EAC49CB353425ULL   /*  380 */,    0xF450244EEF0129D8ULL   /*  381 */,
    0x8ACC46E5CAF4DEB6ULL   /*  382 */,    0x2FFEAB63989263F7ULL   /*  383 */,
    0x8F7CB9FE5D7A4578ULL   /*  384 */,    0x5BD8F7644E634635ULL   /*  385 */,
    0x427A7315BF2DC900ULL   /*  386 */,    0x17D0C4AA2125261CULL   /*  387 */,
    0x3992486C93518E50ULL   /*  388 */,    0xB4CBFEE0A2D7D4C3ULL   /*  389 */,
    0x7C75D6202C5DDD8DULL   /*  390 */,    0xDBC295D8E35B6C61ULL   /*  391 */,
    0x60B369D302032B19ULL   /*  392 */,    0xCE42685FDCE44132ULL   /*  393 */,
    0x06F3DDB9DDF65610ULL   /*  394 */,    0x8EA4D21DB5E148F0ULL   /*  395 */,
    0x20B0FCE62FCD496FULL   /*  396 */,    0x2C1B912358B0EE31ULL   /*  397 */,
    0xB28317B818F5A308ULL   /*  398 */,    0xA89C1E189CA6D2CFULL   /*  399 */,
    0x0C6B18576AAADBC8ULL   /*  400 */,    0xB65DEAA91299FAE3ULL   /*  401 */,
    0xFB2B794B7F1027E7ULL   /*  402 */,    0x04E4317F443B5BEBULL   /*  403 */,
    0x4B852D325939D0A6ULL   /*  404 */,    0xD5AE6BEEFB207FFCULL   /*  405 */,
    0x309682B281C7D374ULL   /*  406 */,    0xBAE309A194C3B475ULL   /*  407 */,
    0x8CC3F97B13B49F05ULL   /*  408 */,    0x98A9422FF8293967ULL   /*  409 */,
    0x244B16B01076FF7CULL   /*  410 */,    0xF8BF571C663D67EEULL   /*  411 */,
    0x1F0D6758EEE30DA1ULL   /*  412 */,    0xC9B611D97ADEB9B7ULL   /*  413 */,
    0xB7AFD5887B6C57A2ULL   /*  414 */,    0x6290AE846B984FE1ULL   /*  415 */,
    0x94DF4CDEACC1A5FDULL   /*  416 */,    0x058A5BD1C5483AFFULL   /*  417 */,
    0x63166CC142BA3C37ULL   /*  418 */,    0x8DB8526EB2F76F40ULL   /*  419 */,
    0xE10880036F0D6D4EULL   /*  420 */,    0x9E0523C9971D311DULL   /*  421 */,
    0x45EC2824CC7CD691ULL   /*  422 */,    0x575B8359E62382C9ULL   /*  423 */,
    0xFA9E400DC4889995ULL   /*  424 */,    0xD1823ECB45721568ULL   /*  425 */,
    0xDAFD983B8206082FULL   /*  426 */,    0xAA7D29082386A8CBULL   /*  427 */,
    0x269FCD4403B87588ULL   /*  428 */,    0x1B91F5F728BDD1E0ULL   /*  429 */,
    0xE4669F39040201F6ULL   /*  430 */,    0x7A1D7C218CF04ADEULL   /*  431 */,
    0x65623C29D79CE5CEULL   /*  432 */,    0x2368449096C00BB1ULL   /*  433 */,
    0xAB9BF1879DA503BAULL   /*  434 */,    0xBC23ECB1A458058EULL   /*  435 */,
    0x9A58DF01BB401ECCULL   /*  436 */,    0xA070E868A85F143DULL   /*  437 */,
    0x4FF188307DF2239EULL   /*  438 */,    0x14D565B41A641183ULL   /*  439 */,
    0xEE13337452701602ULL   /*  440 */,    0x950E3DCF3F285E09ULL   /*  441 */,
    0x59930254B9C80953ULL   /*  442 */,    0x3BF299408930DA6DULL   /*  443 */,
    0xA955943F53691387ULL   /*  444 */,    0xA15EDECAA9CB8784ULL   /*  445 */,
    0x29142127352BE9A0ULL   /*  446 */,    0x76F0371FFF4E7AFBULL   /*  447 */,
    0x0239F450274F2228ULL   /*  448 */,    0xBB073AF01D5E868BULL   /*  449 */,
    0xBFC80571C10E96C1ULL   /*  450 */,    0xD267088568222E23ULL   /*  451 */,
    0x9671A3D48E80B5B0ULL   /*  452 */,    0x55B5D38AE193BB81ULL   /*  453 */,
    0x693AE2D0A18B04B8ULL   /*  454 */,    0x5C48B4ECADD5335FULL   /*  455 */,
    0xFD743B194916A1CAULL   /*  456 */,    0x2577018134BE98C4ULL   /*  457 */,
    0xE77987E83C54A4ADULL   /*  458 */,    0x28E11014DA33E1B9ULL   /*  459 */,
    0x270CC59E226AA213ULL   /*  460 */,    0x71495F756D1A5F60ULL   /*  461 */,
    0x9BE853FB60AFEF77ULL   /*  462 */,    0xADC786A7F7443DBFULL   /*  463 */,
    0x0904456173B29A82ULL   /*  464 */,    0x58BC7A66C232BD5EULL   /*  465 */,
    0xF306558C673AC8B2ULL   /*  466 */,    0x41F639C6B6C9772AULL   /*  467 */,
    0x216DEFE99FDA35DAULL   /*  468 */,    0x11640CC71C7BE615ULL   /*  469 */,
    0x93C43694565C5527ULL   /*  470 */,    0xEA038E6246777839ULL   /*  471 */,
    0xF9ABF3CE5A3E2469ULL   /*  472 */,    0x741E768D0FD312D2ULL   /*  473 */,
    0x0144B883CED652C6ULL   /*  474 */,    0xC20B5A5BA33F8552ULL   /*  475 */,
    0x1AE69633C3435A9DULL   /*  476 */,    0x97A28CA4088CFDECULL   /*  477 */,
    0x8824A43C1E96F420ULL   /*  478 */,    0x37612FA66EEEA746ULL   /*  479 */,
    0x6B4CB165F9CF0E5AULL   /*  480 */,    0x43AA1C06A0ABFB4AULL   /*  481 */,
    0x7F4DC26FF162796BULL   /*  482 */,    0x6CBACC8E54ED9B0FULL   /*  483 */,
    0xA6B7FFEFD2BB253EULL   /*  484 */,    0x2E25BC95B0A29D4FULL   /*  485 */,
    0x86D6A58BDEF1388CULL   /*  486 */,    0xDED74AC576B6F054ULL   /*  487 */,
    0x8030BDBC2B45805DULL   /*  488 */,    0x3C81AF70E94D9289ULL   /*  489 */,
    0x3EFF6DDA9E3100DBULL   /*  490 */,    0xB38DC39FDFCC8847ULL   /*  491 */,
    0x123885528D17B87EULL   /*  492 */,    0xF2DA0ED240B1B642ULL   /*  493 */,
    0x44CEFADCD54BF9A9ULL   /*  494 */,    0x1312200E433C7EE6ULL   /*  495 */,
    0x9FFCC84F3A78C748ULL   /*  496 */,    0xF0CD1F72248576BBULL   /*  497 */,
    0xEC6974053638CFE4ULL   /*  498 */,    0x2BA7B67C0CEC4E4CULL   /*  499 */,
    0xAC2F4DF3E5CE32EDULL   /*  500 */,    0xCB33D14326EA4C11ULL   /*  501 */,
    0xA4E9044CC77E58BCULL   /*  502 */,    0x5F513293D934FCEFULL   /*  503 */,
    0x5DC9645506E55444ULL   /*  504 */,    0x50DE418F317DE40AULL   /*  505 */,
    0x388CB31A69DDE259ULL   /*  506 */,    0x2DB4A83455820A86ULL   /*  507 */,
    0x9010A91E84711AE9ULL   /*  508 */,    0x4DF7F0B7B1498371ULL   /*  509 */,
    0xD62A2EABC0977179ULL   /*  510 */,    0x22FAC097AA8D5C0EULL   /*  511 */,
    0xF49FCC2FF1DAF39BULL   /*  512 */,    0x487FD5C66FF29281ULL   /*  513 */,
    0xE8A30667FCDCA83FULL   /*  514 */,    0x2C9B4BE3D2FCCE63ULL   /*  515 */,
    0xDA3FF74B93FBBBC2ULL   /*  516 */,    0x2FA165D2FE70BA66ULL   /*  517 */,
    0xA103E279970E93D4ULL   /*  518 */,    0xBECDEC77B0E45E71ULL   /*  519 */,
    0xCFB41E723985E497ULL   /*  520 */,    0xB70AAA025EF75017ULL   /*  521 */,
    0xD42309F03840B8E0ULL   /*  522 */,    0x8EFC1AD035898579ULL   /*  523 */,
    0x96C6920BE2B2ABC5ULL   /*  524 */,    0x66AF4163375A9172ULL   /*  525 */,
    0x2174ABDCCA7127FBULL   /*  526 */,    0xB33CCEA64A72FF41ULL   /*  527 */,
    0xF04A4933083066A5ULL   /*  528 */,    0x8D970ACDD7289AF5ULL   /*  529 */,
    0x8F96E8E031C8C25EULL   /*  530 */,    0xF3FEC02276875D47ULL   /*  531 */,
    0xEC7BF310056190DDULL   /*  532 */,    0xF5ADB0AEBB0F1491ULL   /*  533 */,
    0x9B50F8850FD58892ULL   /*  534 */,    0x4975488358B74DE8ULL   /*  535 */,
    0xA3354FF691531C61ULL   /*  536 */,    0x0702BBE481D2C6EEULL   /*  537 */,
    0x89FB24057DEDED98ULL   /*  538 */,    0xAC3075138596E902ULL   /*  539 */,
    0x1D2D3580172772EDULL   /*  540 */,    0xEB738FC28E6BC30DULL   /*  541 */,
    0x5854EF8F63044326ULL   /*  542 */,    0x9E5C52325ADD3BBEULL   /*  543 */,
    0x90AA53CF325C4623ULL   /*  544 */,    0xC1D24D51349DD067ULL   /*  545 */,
    0x2051CFEEA69EA624ULL   /*  546 */,    0x13220F0A862E7E4FULL   /*  547 */,
    0xCE39399404E04864ULL   /*  548 */,    0xD9C42CA47086FCB7ULL   /*  549 */,
    0x685AD2238A03E7CCULL   /*  550 */,    0x066484B2AB2FF1DBULL   /*  551 */,
    0xFE9D5D70EFBF79ECULL   /*  552 */,    0x5B13B9DD9C481854ULL   /*  553 */,
    0x15F0D475ED1509ADULL   /*  554 */,    0x0BEBCD060EC79851ULL   /*  555 */,
    0xD58C6791183AB7F8ULL   /*  556 */,    0xD1187C5052F3EEE4ULL   /*  557 */,
    0xC95D1192E54E82FFULL   /*  558 */,    0x86EEA14CB9AC6CA2ULL   /*  559 */,
    0x3485BEB153677D5DULL   /*  560 */,    0xDD191D781F8C492AULL   /*  561 */,
    0xF60866BAA784EBF9ULL   /*  562 */,    0x518F643BA2D08C74ULL   /*  563 */,
    0x8852E956E1087C22ULL   /*  564 */,    0xA768CB8DC410AE8DULL   /*  565 */,
    0x38047726BFEC8E1AULL   /*  566 */,    0xA67738B4CD3B45AAULL   /*  567 */,
    0xAD16691CEC0DDE19ULL   /*  568 */,    0xC6D4319380462E07ULL   /*  569 */,
    0xC5A5876D0BA61938ULL   /*  570 */,    0x16B9FA1FA58FD840ULL   /*  571 */,
    0x188AB1173CA74F18ULL   /*  572 */,    0xABDA2F98C99C021FULL   /*  573 */,
    0x3E0580AB134AE816ULL   /*  574 */,    0x5F3B05B773645ABBULL   /*  575 */,
    0x2501A2BE5575F2F6ULL   /*  576 */,    0x1B2F74004E7E8BA9ULL   /*  577 */,
    0x1CD7580371E8D953ULL   /*  578 */,    0x7F6ED89562764E30ULL   /*  579 */,
    0xB15926FF596F003DULL   /*  580 */,    0x9F65293DA8C5D6B9ULL   /*  581 */,
    0x6ECEF04DD690F84CULL   /*  582 */,    0x4782275FFF33AF88ULL   /*  583 */,
    0xE41433083F820801ULL   /*  584 */,    0xFD0DFE409A1AF9B5ULL   /*  585 */,
    0x4325A3342CDB396BULL   /*  586 */,    0x8AE77E62B301B252ULL   /*  587 */,
    0xC36F9E9F6655615AULL   /*  588 */,    0x85455A2D92D32C09ULL   /*  589 */,
    0xF2C7DEA949477485ULL   /*  590 */,    0x63CFB4C133A39EBAULL   /*  591 */,
    0x83B040CC6EBC5462ULL   /*  592 */,    0x3B9454C8FDB326B0ULL   /*  593 */,
    0x56F56A9E87FFD78CULL   /*  594 */,    0x2DC2940D99F42BC6ULL   /*  595 */,
    0x98F7DF096B096E2DULL   /*  596 */,    0x19A6E01E3AD852BFULL   /*  597 */,
    0x42A99CCBDBD4B40BULL   /*  598 */,    0xA59998AF45E9C559ULL   /*  599 */,
    0x366295E807D93186ULL   /*  600 */,    0x6B48181BFAA1F773ULL   /*  601 */,
    0x1FEC57E2157A0A1DULL   /*  602 */,    0x4667446AF6201AD5ULL   /*  603 */,
    0xE615EBCACFB0F075ULL   /*  604 */,    0xB8F31F4F68290778ULL   /*  605 */,
    0x22713ED6CE22D11EULL   /*  606 */,    0x3057C1A72EC3C93BULL   /*  607 */,
    0xCB46ACC37C3F1F2FULL   /*  608 */,    0xDBB893FD02AAF50EULL   /*  609 */,
    0x331FD92E600B9FCFULL   /*  610 */,    0xA498F96148EA3AD6ULL   /*  611 */,
    0xA8D8426E8B6A83EAULL   /*  612 */,    0xA089B274B7735CDCULL   /*  613 */,
    0x87F6B3731E524A11ULL   /*  614 */,    0x118808E5CBC96749ULL   /*  615 */,
    0x9906E4C7B19BD394ULL   /*  616 */,    0xAFED7F7E9B24A20CULL   /*  617 */,
    0x6509EADEEB3644A7ULL   /*  618 */,    0x6C1EF1D3E8EF0EDEULL   /*  619 */,
    0xB9C97D43E9798FB4ULL   /*  620 */,    0xA2F2D784740C28A3ULL   /*  621 */,
    0x7B8496476197566FULL   /*  622 */,    0x7A5BE3E6B65F069DULL   /*  623 */,
    0xF96330ED78BE6F10ULL   /*  624 */,    0xEEE60DE77A076A15ULL   /*  625 */,
    0x2B4BEE4AA08B9BD0ULL   /*  626 */,    0x6A56A63EC7B8894EULL   /*  627 */,
    0x02121359BA34FEF4ULL   /*  628 */,    0x4CBF99F8283703FCULL   /*  629 */,
    0x398071350CAF30C8ULL   /*  630 */,    0xD0A77A89F017687AULL   /*  631 */,
    0xF1C1A9EB9E423569ULL   /*  632 */,    0x8C7976282DEE8199ULL   /*  633 */,
    0x5D1737A5DD1F7ABDULL   /*  634 */,    0x4F53433C09A9FA80ULL   /*  635 */,
    0xFA8B0C53DF7CA1D9ULL   /*  636 */,    0x3FD9DCBC886CCB77ULL   /*  637 */,
    0xC040917CA91B4720ULL   /*  638 */,    0x7DD00142F9D1DCDFULL   /*  639 */,
    0x8476FC1D4F387B58ULL   /*  640 */,    0x23F8E7C5F3316503ULL   /*  641 */,
    0x032A2244E7E37339ULL   /*  642 */,    0x5C87A5D750F5A74BULL   /*  643 */,
    0x082B4CC43698992EULL   /*  644 */,    0xDF917BECB858F63CULL   /*  645 */,
    0x3270B8FC5BF86DDAULL   /*  646 */,    0x10AE72BB29B5DD76ULL   /*  647 */,
    0x576AC94E7700362BULL   /*  648 */,    0x1AD112DAC61EFB8FULL   /*  649 */,
    0x691BC30EC5FAA427ULL   /*  650 */,    0xFF246311CC327143ULL   /*  651 */,
    0x3142368E30E53206ULL   /*  652 */,    0x71380E31E02CA396ULL   /*  653 */,
    0x958D5C960AAD76F1ULL   /*  654 */,    0xF8D6F430C16DA536ULL   /*  655 */,
    0xC8FFD13F1BE7E1D2ULL   /*  656 */,    0x7578AE66004DDBE1ULL   /*  657 */,
    0x05833F01067BE646ULL   /*  658 */,    0xBB34B5AD3BFE586DULL   /*  659 */,
    0x095F34C9A12B97F0ULL   /*  660 */,    0x247AB64525D60CA8ULL   /*  661 */,
    0xDCDBC6F3017477D1ULL   /*  662 */,    0x4A2E14D4DECAD24DULL   /*  663 */,
    0xBDB5E6D9BE0A1EEBULL   /*  664 */,    0x2A7E70F7794301ABULL   /*  665 */,
    0xDEF42D8A270540FDULL   /*  666 */,    0x01078EC0A34C22C1ULL   /*  667 */,
    0xE5DE511AF4C16387ULL   /*  668 */,    0x7EBB3A52BD9A330AULL   /*  669 */,
    0x77697857AA7D6435ULL   /*  670 */,    0x004E831603AE4C32ULL   /*  671 */,
    0xE7A21020AD78E312ULL   /*  672 */,    0x9D41A70C6AB420F2ULL   /*  673 */,
    0x28E06C18EA1141E6ULL   /*  674 */,    0xD2B28CBD984F6B28ULL   /*  675 */,
    0x26B75F6C446E9D83ULL   /*  676 */,    0xBA47568C4D418D7FULL   /*  677 */,
    0xD80BADBFE6183D8EULL   /*  678 */,    0x0E206D7F5F166044ULL   /*  679 */,
    0xE258A43911CBCA3EULL   /*  680 */,    0x723A1746B21DC0BCULL   /*  681 */,
    0xC7CAA854F5D7CDD3ULL   /*  682 */,    0x7CAC32883D261D9CULL   /*  683 */,
    0x7690C26423BA942CULL   /*  684 */,    0x17E55524478042B8ULL   /*  685 */,
    0xE0BE477656A2389FULL   /*  686 */,    0x4D289B5E67AB2DA0ULL   /*  687 */,
    0x44862B9C8FBBFD31ULL   /*  688 */,    0xB47CC8049D141365ULL   /*  689 */,
    0x822C1B362B91C793ULL   /*  690 */,    0x4EB14655FB13DFD8ULL   /*  691 */,
    0x1ECBBA0714E2A97BULL   /*  692 */,    0x6143459D5CDE5F14ULL   /*  693 */,
    0x53A8FBF1D5F0AC89ULL   /*  694 */,    0x97EA04D81C5E5B00ULL   /*  695 */,
    0x622181A8D4FDB3F3ULL   /*  696 */,    0xE9BCD341572A1208ULL   /*  697 */,
    0x1411258643CCE58AULL   /*  698 */,    0x9144C5FEA4C6E0A4ULL   /*  699 */,
    0x0D33D06565CF620FULL   /*  700 */,    0x54A48D489F219CA1ULL   /*  701 */,
    0xC43E5EAC6D63C821ULL   /*  702 */,    0xA9728B3A72770DAFULL   /*  703 */,
    0xD7934E7B20DF87EFULL   /*  704 */,    0xE35503B61A3E86E5ULL   /*  705 */,
    0xCAE321FBC819D504ULL   /*  706 */,    0x129A50B3AC60BFA6ULL   /*  707 */,
    0xCD5E68EA7E9FB6C3ULL   /*  708 */,    0xB01C90199483B1C7ULL   /*  709 */,
    0x3DE93CD5C295376CULL   /*  710 */,    0xAED52EDF2AB9AD13ULL   /*  711 */,
    0x2E60F512C0A07884ULL   /*  712 */,    0xBC3D86A3E36210C9ULL   /*  713 */,
    0x35269D9B163951CEULL   /*  714 */,    0x0C7D6E2AD0CDB5FAULL   /*  715 */,
    0x59E86297D87F5733ULL   /*  716 */,    0x298EF221898DB0E7ULL   /*  717 */,
    0x55000029D1A5AA7EULL   /*  718 */,    0x8BC08AE1B5061B45ULL   /*  719 */,
    0xC2C31C2B6C92703AULL   /*  720 */,    0x94CC596BAF25EF42ULL   /*  721 */,
    0x0A1D73DB22540456ULL   /*  722 */,    0x04B6A0F9D9C4179AULL   /*  723 */,
    0xEFFDAFA2AE3D3C60ULL   /*  724 */,    0xF7C8075BB49496C4ULL   /*  725 */,
    0x9CC5C7141D1CD4E3ULL   /*  726 */,    0x78BD1638218E5534ULL   /*  727 */,
    0xB2F11568F850246AULL   /*  728 */,    0xEDFABCFA9502BC29ULL   /*  729 */,
    0x796CE5F2DA23051BULL   /*  730 */,    0xAAE128B0DC93537CULL   /*  731 */,
    0x3A493DA0EE4B29AEULL   /*  732 */,    0xB5DF6B2C416895D7ULL   /*  733 */,
    0xFCABBD25122D7F37ULL   /*  734 */,    0x70810B58105DC4B1ULL   /*  735 */,
    0xE10FDD37F7882A90ULL   /*  736 */,    0x524DCAB5518A3F5CULL   /*  737 */,
    0x3C9E85878451255BULL   /*  738 */,    0x4029828119BD34E2ULL   /*  739 */,
    0x74A05B6F5D3CECCBULL   /*  740 */,    0xB610021542E13ECAULL   /*  741 */,
    0x0FF979D12F59E2ACULL   /*  742 */,    0x6037DA27E4F9CC50ULL   /*  743 */,
    0x5E92975A0DF1847DULL   /*  744 */,    0xD66DE190D3E623FEULL   /*  745 */,
    0x5032D6B87B568048ULL   /*  746 */,    0x9A36B7CE8235216EULL   /*  747 */,
    0x80272A7A24F64B4AULL   /*  748 */,    0x93EFED8B8C6916F7ULL   /*  749 */,
    0x37DDBFF44CCE1555ULL   /*  750 */,    0x4B95DB5D4B99BD25ULL   /*  751 */,
    0x92D3FDA169812FC0ULL   /*  752 */,    0xFB1A4A9A90660BB6ULL   /*  753 */,
    0x730C196946A4B9B2ULL   /*  754 */,    0x81E289AA7F49DA68ULL   /*  755 */,
    0x64669A0F83B1A05FULL   /*  756 */,    0x27B3FF7D9644F48BULL   /*  757 */,
    0xCC6B615C8DB675B3ULL   /*  758 */,    0x674F20B9BCEBBE95ULL   /*  759 */,
    0x6F31238275655982ULL   /*  760 */,    0x5AE488713E45CF05ULL   /*  761 */,
    0xBF619F9954C21157ULL   /*  762 */,    0xEABAC46040A8EAE9ULL   /*  763 */,
    0x454C6FE9F2C0C1CDULL   /*  764 */,    0x419CF6496412691CULL   /*  765 */,
    0xD3DC3BEF265B0F70ULL   /*  766 */,    0x6D0E60F5C3578A9EULL   /*  767 */,
    0x5B0E608526323C55ULL   /*  768 */,    0x1A46C1A9FA1B59F5ULL   /*  769 */,
    0xA9E245A17C4C8FFAULL   /*  770 */,    0x65CA5159DB2955D7ULL   /*  771 */,
    0x05DB0A76CE35AFC2ULL   /*  772 */,    0x81EAC77EA9113D45ULL   /*  773 */,
    0x528EF88AB6AC0A0DULL   /*  774 */,    0xA09EA253597BE3FFULL   /*  775 */,
    0x430DDFB3AC48CD56ULL   /*  776 */,    0xC4B3A67AF45CE46FULL   /*  777 */,
    0x4ECECFD8FBE2D05EULL   /*  778 */,    0x3EF56F10B39935F0ULL   /*  779 */,
    0x0B22D6829CD619C6ULL   /*  780 */,    0x17FD460A74DF2069ULL   /*  781 */,
    0x6CF8CC8E8510ED40ULL   /*  782 */,    0xD6C824BF3A6ECAA7ULL   /*  783 */,
    0x61243D581A817049ULL   /*  784 */,    0x048BACB6BBC163A2ULL   /*  785 */,
    0xD9A38AC27D44CC32ULL   /*  786 */,    0x7FDDFF5BAAF410ABULL   /*  787 */,
    0xAD6D495AA804824BULL   /*  788 */,    0xE1A6A74F2D8C9F94ULL   /*  789 */,
    0xD4F7851235DEE8E3ULL   /*  790 */,    0xFD4B7F886540D893ULL   /*  791 */,
    0x247C20042AA4BFDAULL   /*  792 */,    0x096EA1C517D1327CULL   /*  793 */,
    0xD56966B4361A6685ULL   /*  794 */,    0x277DA5C31221057DULL   /*  795 */,
    0x94D59893A43ACFF7ULL   /*  796 */,    0x64F0C51CCDC02281ULL   /*  797 */,
    0x3D33BCC4FF6189DBULL   /*  798 */,    0xE005CB184CE66AF1ULL   /*  799 */,
    0xFF5CCD1D1DB99BEAULL   /*  800 */,    0xB0B854A7FE42980FULL   /*  801 */,
    0x7BD46A6A718D4B9FULL   /*  802 */,    0xD10FA8CC22A5FD8CULL   /*  803 */,
    0xD31484952BE4BD31ULL   /*  804 */,    0xC7FA975FCB243847ULL   /*  805 */,
    0x4886ED1E5846C407ULL   /*  806 */,    0x28CDDB791EB70B04ULL   /*  807 */,
    0xC2B00BE2F573417FULL   /*  808 */,    0x5C9590452180F877ULL   /*  809 */,
    0x7A6BDDFFF370EB00ULL   /*  810 */,    0xCE509E38D6D9D6A4ULL   /*  811 */,
    0xEBEB0F00647FA702ULL   /*  812 */,    0x1DCC06CF76606F06ULL   /*  813 */,
    0xE4D9F28BA286FF0AULL   /*  814 */,    0xD85A305DC918C262ULL   /*  815 */,
    0x475B1D8732225F54ULL   /*  816 */,    0x2D4FB51668CCB5FEULL   /*  817 */,
    0xA679B9D9D72BBA20ULL   /*  818 */,    0x53841C0D912D43A5ULL   /*  819 */,
    0x3B7EAA48BF12A4E8ULL   /*  820 */,    0x781E0E47F22F1DDFULL   /*  821 */,
    0xEFF20CE60AB50973ULL   /*  822 */,    0x20D261D19DFFB742ULL   /*  823 */,
    0x16A12B03062A2E39ULL   /*  824 */,    0x1960EB2239650495ULL   /*  825 */,
    0x251C16FED50EB8B8ULL   /*  826 */,    0x9AC0C330F826016EULL   /*  827 */,
    0xED152665953E7671ULL   /*  828 */,    0x02D63194A6369570ULL   /*  829 */,
    0x5074F08394B1C987ULL   /*  830 */,    0x70BA598C90B25CE1ULL   /*  831 */,
    0x794A15810B9742F6ULL   /*  832 */,    0x0D5925E9FCAF8C6CULL   /*  833 */,
    0x3067716CD868744EULL   /*  834 */,    0x910AB077E8D7731BULL   /*  835 */,
    0x6A61BBDB5AC42F61ULL   /*  836 */,    0x93513EFBF0851567ULL   /*  837 */,
    0xF494724B9E83E9D5ULL   /*  838 */,    0xE887E1985C09648DULL   /*  839 */,
    0x34B1D3C675370CFDULL   /*  840 */,    0xDC35E433BC0D255DULL   /*  841 */,
    0xD0AAB84234131BE0ULL   /*  842 */,    0x08042A50B48B7EAFULL   /*  843 */,
    0x9997C4EE44A3AB35ULL   /*  844 */,    0x829A7B49201799D0ULL   /*  845 */,
    0x263B8307B7C54441ULL   /*  846 */,    0x752F95F4FD6A6CA6ULL   /*  847 */,
    0x927217402C08C6E5ULL   /*  848 */,    0x2A8AB754A795D9EEULL   /*  849 */,
    0xA442F7552F72943DULL   /*  850 */,    0x2C31334E19781208ULL   /*  851 */,
    0x4FA98D7CEAEE6291ULL   /*  852 */,    0x55C3862F665DB309ULL   /*  853 */,
    0xBD0610175D53B1F3ULL   /*  854 */,    0x46FE6CB840413F27ULL   /*  855 */,
    0x3FE03792DF0CFA59ULL   /*  856 */,    0xCFE700372EB85E8FULL   /*  857 */,
    0xA7BE29E7ADBCE118ULL   /*  858 */,    0xE544EE5CDE8431DDULL   /*  859 */,
    0x8A781B1B41F1873EULL   /*  860 */,    0xA5C94C78A0D2F0E7ULL   /*  861 */,
    0x39412E2877B60728ULL   /*  862 */,    0xA1265EF3AFC9A62CULL   /*  863 */,
    0xBCC2770C6A2506C5ULL   /*  864 */,    0x3AB66DD5DCE1CE12ULL   /*  865 */,
    0xE65499D04A675B37ULL   /*  866 */,    0x7D8F523481BFD216ULL   /*  867 */,
    0x0F6F64FCEC15F389ULL   /*  868 */,    0x74EFBE618B5B13C8ULL   /*  869 */,
    0xACDC82B714273E1DULL   /*  870 */,    0xDD40BFE003199D17ULL   /*  871 */,
    0x37E99257E7E061F8ULL   /*  872 */,    0xFA52626904775AAAULL   /*  873 */,
    0x8BBBF63A463D56F9ULL   /*  874 */,    0xF0013F1543A26E64ULL   /*  875 */,
    0xA8307E9F879EC898ULL   /*  876 */,    0xCC4C27A4150177CCULL   /*  877 */,
    0x1B432F2CCA1D3348ULL   /*  878 */,    0xDE1D1F8F9F6FA013ULL   /*  879 */,
    0x606602A047A7DDD6ULL   /*  880 */,    0xD237AB64CC1CB2C7ULL   /*  881 */,
    0x9B938E7225FCD1D3ULL   /*  882 */,    0xEC4E03708E0FF476ULL   /*  883 */,
    0xFEB2FBDA3D03C12DULL   /*  884 */,    0xAE0BCED2EE43889AULL   /*  885 */,
    0x22CB8923EBFB4F43ULL   /*  886 */,    0x69360D013CF7396DULL   /*  887 */,
    0x855E3602D2D4E022ULL   /*  888 */,    0x073805BAD01F784CULL   /*  889 */,
    0x33E17A133852F546ULL   /*  890 */,    0xDF4874058AC7B638ULL   /*  891 */,
    0xBA92B29C678AA14AULL   /*  892 */,    0x0CE89FC76CFAADCDULL   /*  893 */,
    0x5F9D4E0908339E34ULL   /*  894 */,    0xF1AFE9291F5923B9ULL   /*  895 */,
    0x6E3480F60F4A265FULL   /*  896 */,    0xEEBF3A2AB29B841CULL   /*  897 */,
    0xE21938A88F91B4ADULL   /*  898 */,    0x57DFEFF845C6D3C3ULL   /*  899 */,
    0x2F006B0BF62CAAF2ULL   /*  900 */,    0x62F479EF6F75EE78ULL   /*  901 */,
    0x11A55AD41C8916A9ULL   /*  902 */,    0xF229D29084FED453ULL   /*  903 */,
    0x42F1C27B16B000E6ULL   /*  904 */,    0x2B1F76749823C074ULL   /*  905 */,
    0x4B76ECA3C2745360ULL   /*  906 */,    0x8C98F463B91691BDULL   /*  907 */,
    0x14BCC93CF1ADE66AULL   /*  908 */,    0x8885213E6D458397ULL   /*  909 */,
    0x8E177DF0274D4711ULL   /*  910 */,    0xB49B73B5503F2951ULL   /*  911 */,
    0x10168168C3F96B6BULL   /*  912 */,    0x0E3D963B63CAB0AEULL   /*  913 */,
    0x8DFC4B5655A1DB14ULL   /*  914 */,    0xF789F1356E14DE5CULL   /*  915 */,
    0x683E68AF4E51DAC1ULL   /*  916 */,    0xC9A84F9D8D4B0FD9ULL   /*  917 */,
    0x3691E03F52A0F9D1ULL   /*  918 */,    0x5ED86E46E1878E80ULL   /*  919 */,
    0x3C711A0E99D07150ULL   /*  920 */,    0x5A0865B20C4E9310ULL   /*  921 */,
    0x56FBFC1FE4F0682EULL   /*  922 */,    0xEA8D5DE3105EDF9BULL   /*  923 */,
    0x71ABFDB12379187AULL   /*  924 */,    0x2EB99DE1BEE77B9CULL   /*  925 */,
    0x21ECC0EA33CF4523ULL   /*  926 */,    0x59A4D7521805C7A1ULL   /*  927 */,
    0x3896F5EB56AE7C72ULL   /*  928 */,    0xAA638F3DB18F75DCULL   /*  929 */,
    0x9F39358DABE9808EULL   /*  930 */,    0xB7DEFA91C00B72ACULL   /*  931 */,
    0x6B5541FD62492D92ULL   /*  932 */,    0x6DC6DEE8F92E4D5BULL   /*  933 */,
    0x353F57ABC4BEEA7EULL   /*  934 */,    0x735769D6DA5690CEULL   /*  935 */,
    0x0A234AA642391484ULL   /*  936 */,    0xF6F9508028F80D9DULL   /*  937 */,
    0xB8E319A27AB3F215ULL   /*  938 */,    0x31AD9C1151341A4DULL   /*  939 */,
    0x773C22A57BEF5805ULL   /*  940 */,    0x45C7561A07968633ULL   /*  941 */,
    0xF913DA9E249DBE36ULL   /*  942 */,    0xDA652D9B78A64C68ULL   /*  943 */,
    0x4C27A97F3BC334EFULL   /*  944 */,    0x76621220E66B17F4ULL   /*  945 */,
    0x967743899ACD7D0BULL   /*  946 */,    0xF3EE5BCAE0ED6782ULL   /*  947 */,
    0x409F753600C879FCULL   /*  948 */,    0x06D09A39B5926DB6ULL   /*  949 */,
    0x6F83AEB0317AC588ULL   /*  950 */,    0x01E6CA4A86381F21ULL   /*  951 */,
    0x66FF3462D19F3025ULL   /*  952 */,    0x72207C24DDFD3BFBULL   /*  953 */,
    0x4AF6B6D3E2ECE2EBULL   /*  954 */,    0x9C994DBEC7EA08DEULL   /*  955 */,
    0x49ACE597B09A8BC4ULL   /*  956 */,    0xB38C4766CF0797BAULL   /*  957 */,
    0x131B9373C57C2A75ULL   /*  958 */,    0xB1822CCE61931E58ULL   /*  959 */,
    0x9D7555B909BA1C0CULL   /*  960 */,    0x127FAFDD937D11D2ULL   /*  961 */,
    0x29DA3BADC66D92E4ULL   /*  962 */,    0xA2C1D57154C2ECBCULL   /*  963 */,
    0x58C5134D82F6FE24ULL   /*  964 */,    0x1C3AE3515B62274FULL   /*  965 */,
    0xE907C82E01CB8126ULL   /*  966 */,    0xF8ED091913E37FCBULL   /*  967 */,
    0x3249D8F9C80046C9ULL   /*  968 */,    0x80CF9BEDE388FB63ULL   /*  969 */,
    0x1881539A116CF19EULL   /*  970 */,    0x5103F3F76BD52457ULL   /*  971 */,
    0x15B7E6F5AE47F7A8ULL   /*  972 */,    0xDBD7C6DED47E9CCFULL   /*  973 */,
    0x44E55C410228BB1AULL   /*  974 */,    0xB647D4255EDB4E99ULL   /*  975 */,
    0x5D11882BB8AAFC30ULL   /*  976 */,    0xF5098BBB29D3212AULL   /*  977 */,
    0x8FB5EA14E90296B3ULL   /*  978 */,    0x677B942157DD025AULL   /*  979 */,
    0xFB58E7C0A390ACB5ULL   /*  980 */,    0x89D3674C83BD4A01ULL   /*  981 */,
    0x9E2DA4DF4BF3B93BULL   /*  982 */,    0xFCC41E328CAB4829ULL   /*  983 */,
    0x03F38C96BA582C52ULL   /*  984 */,    0xCAD1BDBD7FD85DB2ULL   /*  985 */,
    0xBBB442C16082AE83ULL   /*  986 */,    0xB95FE86BA5DA9AB0ULL   /*  987 */,
    0xB22E04673771A93FULL   /*  988 */,    0x845358C9493152D8ULL   /*  989 */,
    0xBE2A488697B4541EULL   /*  990 */,    0x95A2DC2DD38E6966ULL   /*  991 */,
    0xC02C11AC923C852BULL   /*  992 */,    0x2388B1990DF2A87BULL   /*  993 */,
    0x7C8008FA1B4F37BEULL   /*  994 */,    0x1F70D0C84D54E503ULL   /*  995 */,
    0x5490ADEC7ECE57D4ULL   /*  996 */,    0x002B3C27D9063A3AULL   /*  997 */,
    0x7EAEA3848030A2BFULL   /*  998 */,    0xC602326DED2003C0ULL   /*  999 */,
    0x83A7287D69A94086ULL   /* 1000 */,    0xC57A5FCB30F57A8AULL   /* 1001 */,
    0xB56844E479EBE779ULL   /* 1002 */,    0xA373B40F05DCBCE9ULL   /* 1003 */,
    0xD71A786E88570EE2ULL   /* 1004 */,    0x879CBACDBDE8F6A0ULL   /* 1005 */,
    0x976AD1BCC164A32FULL   /* 1006 */,    0xAB21E25E9666D78BULL   /* 1007 */,
    0x901063AAE5E5C33CULL   /* 1008 */,    0x9818B34448698D90ULL   /* 1009 */,
    0xE36487AE3E1E8ABBULL   /* 1010 */,    0xAFBDF931893BDCB4ULL   /* 1011 */,
    0x6345A0DC5FBBD519ULL   /* 1012 */,    0x8628FE269B9465CAULL   /* 1013 */,
    0x1E5D01603F9C51ECULL   /* 1014 */,    0x4DE44006A15049B7ULL   /* 1015 */,
    0xBF6C70E5F776CBB1ULL   /* 1016 */,    0x411218F2EF552BEDULL   /* 1017 */,
    0xCB0C0708705A36A3ULL   /* 1018 */,    0xE74D14754F986044ULL   /* 1019 */,
    0xCD56D9430EA8280EULL   /* 1020 */,    0xC12591D7535F5065ULL   /* 1021 */,
    0xC83223F1720AEF96ULL   /* 1022 */,    0xC3A0396F7363A51FULL   /* 1023 */};
