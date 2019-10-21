#pragma once
#include <atomic>
#include <memory>
#include <unordered_map>
#include "hw/pvr/pvr_regs.h"
#undef ID
#include "hw/pvr/ta_structs.h"
#include "hw/pvr/Renderer_if.h"

extern u8* vq_codebook;
extern u32 palette_index;
extern u32 palette16_ram[1024];
extern u32 palette32_ram[1024];
extern bool pal_needs_update,fog_needs_update;
extern u32 pal_hash_256[4];
extern u32 pal_hash_16[64];
extern bool KillTex;

extern u32 detwiddle[2][8][1024];

template<class pixel_type>
class PixelBuffer
{
	pixel_type* p_buffer_start;
	pixel_type* p_current_line;
	pixel_type* p_current_pixel;

	u32 pixels_per_line = 0;

public:
   PixelBuffer()
   {
      p_buffer_start = p_current_line = p_current_pixel = NULL;
   }

   ~PixelBuffer()
	{
		deinit();
	}

   void init(u32 width, u32 height)
   {
      deinit();
      p_buffer_start = p_current_line = p_current_pixel = (pixel_type *)malloc(width * height * sizeof(pixel_type));
		this->pixels_per_line = width;
   }

   void deinit()
	{
		if (p_buffer_start != NULL)
		{
			free(p_buffer_start);
			p_buffer_start = p_current_line = p_current_pixel = NULL;
		}
	}

	void steal_data(PixelBuffer &buffer)
	{
		deinit();
		p_buffer_start = p_current_line = p_current_pixel = buffer.p_buffer_start;
		pixels_per_line = buffer.pixels_per_line;
		buffer.p_buffer_start = buffer.p_current_line = buffer.p_current_pixel = NULL;
	}

	__forceinline pixel_type *data(u32 x = 0, u32 y = 0)
	{
		return p_buffer_start + pixels_per_line * y + x;
	}

   __forceinline void prel(u32 x,pixel_type value)
 	{
 		p_current_pixel[x]=value;
 	}

   __forceinline void prel(u32 x,u32 y,pixel_type value)
 	{
 		p_current_pixel[y*pixels_per_line+x]=value;
 	}

   __forceinline void rmovex(u32 value)
	{
		p_current_pixel+=value;
	}
	__forceinline void rmovey(u32 value)
	{
		p_current_line+=pixels_per_line*value;
		p_current_pixel=p_current_line;
	}
	__forceinline void amove(u32 x_m,u32 y_m)
	{
		//p_current_pixel=p_buffer_start;
		p_current_line=p_buffer_start+pixels_per_line*y_m;
		p_current_pixel=p_current_line + x_m;
	}
};

void palette_update(void);

#define clamp(minv,maxv,x) min(maxv,max(minv,x))

// Unpack to 16-bit word

#define ARGB1555( word )	( ((word>>15)&1) | (((word>>10) & 0x1F)<<11)  | (((word>>5) & 0x1F)<<6)  | (((word>>0) & 0x1F)<<1) )

#define ARGB565( word )	( (((word>>0)&0x1F)<<0) | (((word>>5)&0x3F)<<5) | (((word>>11)&0x1F)<<11) )
	
#define ARGB4444( word ) ( (((word>>0)&0xF)<<4) | (((word>>4)&0xF)<<8) | (((word>>8)&0xF)<<12) | (((word>>12)&0xF)<<0) )

#define ARGB8888( word ) ( (((word>>4)&0xF)<<4) | (((word>>12)&0xF)<<8) | (((word>>20)&0xF)<<12) | (((word>>28)&0xF)<<0) )

// Unpack to 32-bit word

#define ARGB1555_32( word )    ( ((word & 0x8000) ? 0xFF000000 : 0) | (((word>>10) & 0x1F)<<3)  | (((word>>5) & 0x1F)<<11)  | (((word>>0) & 0x1F)<<19) )

#define ARGB565_32( word )     ( (((word>>11)&0x1F)<<3) | (((word>>5)&0x3F)<<10) | (((word>>0)&0x1F)<<19) | 0xFF000000 )

#define ARGB4444_32( word ) ( (((word>>12)&0xF)<<28) | (((word>>8)&0xF)<<4) | (((word>>4)&0xF)<<12) | (((word>>0)&0xF)<<20) )

#define ARGB8888_32( word ) ( ((word >> 0) & 0xFF000000) | (((word >> 16) & 0xFF) << 0) | (((word >> 8) & 0xFF) << 8) | ((word & 0xFF) << 16) )

template<class PixelPacker>
__forceinline u32 YUV422(s32 Y,s32 Yu,s32 Yv)
{
	Yu-=128;
	Yv-=128;

	//s32 B = (76283*(Y - 16) + 132252*(Yu - 128))>>16;
	//s32 G = (76283*(Y - 16) - 53281 *(Yv - 128) - 25624*(Yu - 128))>>16;
	//s32 R = (76283*(Y - 16) + 104595*(Yv - 128))>>16;
	
	s32 R = Y + Yv*11/8;            // Y + (Yv-128) * (11/8) ?
	s32 G = Y - (Yu*11 + Yv*22)/32; // Y - (Yu-128) * (11/8) * 0.25 - (Yv-128) * (11/8) * 0.5 ?
	s32 B = Y + Yu*110/64;          // Y + (Yu-128) * (11/8) * 1.25 ?

	return PixelPacker::packRGB(clamp(0,255,R),clamp(0,255,G),clamp(0,255,B));
}

#define twop(x,y,bcx,bcy) (detwiddle[0][bcy][x]+detwiddle[1][bcx][y])

//pixel packers !
struct pp_565
{
	__forceinline static u32 packRGB(u8 R,u8 G,u8 B)
	{
		R>>=3;
		G>>=2;
		B>>=3;
		return (R<<11) | (G<<5) | (B<<0);
	}
};

struct pp_8888
{
	__forceinline static u32 packRGB(u8 R,u8 G,u8 B)
	{
      return (R << 0) | (G << 8) | (B << 16) | 0xFF000000;
	}
};

//pixel convertors !
   #define pixelcvt_start_base(name,x,y,type) template<class PixelPacker> \
		struct name \
		{ \
			static const u32 xpp=x;\
			static const u32 ypp=y;	\
			__forceinline static void Convert(PixelBuffer<type>* pb,u8* data) \
		{

#define pixelcvt_start(name,x,y) pixelcvt_start_base(name, x, y, u16)
#define pixelcvt32_start(name,x,y) pixelcvt_start_base(name, x, y, u32)

#define pixelcvt_size_start(name, x, y) template<class PixelPacker, class pixel_size> \
struct name \
{ \
	static const u32 xpp=x;\
	static const u32 ypp=y;	\
   __forceinline static void Convert(PixelBuffer<pixel_size>* pb,u8* data) \
{

#define pixelcvt_end } }
#define pixelcvt_next(name,x,y) pixelcvt_end;  pixelcvt_start(name,x,y)
//
//Non twiddled
//
// 16-bit pixel buffer
pixelcvt_start(conv565_PL,4,1)
{
   //convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB565(p_in[0]));
	//1,0
	pb->prel(1,ARGB565(p_in[1]));
	//2,0
	pb->prel(2,ARGB565(p_in[2]));
	//3,0
	pb->prel(3,ARGB565(p_in[3]));
}

pixelcvt_next(conv1555_PL,4,1)
{
   //convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB1555(p_in[0]));
	//1,0
	pb->prel(1,ARGB1555(p_in[1]));
	//2,0
	pb->prel(2,ARGB1555(p_in[2]));
	//3,0
	pb->prel(3,ARGB1555(p_in[3]));
}

pixelcvt_next(conv4444_PL,4,1)
{
   //convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB4444(p_in[0]));
	//1,0
	pb->prel(1,ARGB4444(p_in[1]));
	//2,0
	pb->prel(2,ARGB4444(p_in[2]));
	//3,0
	pb->prel(3,ARGB4444(p_in[3]));
}
pixelcvt_next(convBMP_PL,4,1)
{
   u16* p_in=(u16*)data;
	pb->prel(0,ARGB4444(p_in[0]));
	pb->prel(1,ARGB4444(p_in[1]));
	pb->prel(2,ARGB4444(p_in[2]));
	pb->prel(3,ARGB4444(p_in[3]));
}
pixelcvt_end;

// 32-bit pixel buffer
pixelcvt32_start(conv565_PL32,4,1)
{
	//convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB565_32(p_in[0]));
	//1,0
	pb->prel(1,ARGB565_32(p_in[1]));
	//2,0
	pb->prel(2,ARGB565_32(p_in[2]));
	//3,0
	pb->prel(3,ARGB565_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv1555_PL32,4,1)
{
	//convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB1555_32(p_in[0]));
	//1,0
	pb->prel(1,ARGB1555_32(p_in[1]));
	//2,0
	pb->prel(2,ARGB1555_32(p_in[2]));
	//3,0
	pb->prel(3,ARGB1555_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv4444_PL32,4,1)
{
	//convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB4444_32(p_in[0]));
	//1,0
	pb->prel(1,ARGB4444_32(p_in[1]));
	//2,0
	pb->prel(2,ARGB4444_32(p_in[2]));
	//3,0
	pb->prel(3,ARGB4444_32(p_in[3]));
}
pixelcvt_end;

pixelcvt32_start(convYUV_PL,4,1)
{
   //convert 4x1 4444 to 4x1 8888
	u32* p_in=(u32*)data;


	s32 Y0 = (p_in[0]>>8) &255; //
	s32 Yu = (p_in[0]>>0) &255; //p_in[0]
	s32 Y1 = (p_in[0]>>24) &255; //p_in[3]
	s32 Yv = (p_in[0]>>16) &255; //p_in[2]

	//0,0
	pb->prel(0,YUV422<PixelPacker>(Y0,Yu,Yv));
	//1,0
	pb->prel(1,YUV422<PixelPacker>(Y1,Yu,Yv));

	//next 4 bytes
	p_in+=1;

	Y0 = (p_in[0]>>8) &255; //
	Yu = (p_in[0]>>0) &255; //p_in[0]
	Y1 = (p_in[0]>>24) &255; //p_in[3]
	Yv = (p_in[0]>>16) &255; //p_in[2]

	//0,0
	pb->prel(2,YUV422<PixelPacker>(Y0,Yu,Yv));
	//1,0
	pb->prel(3,YUV422<PixelPacker>(Y1,Yu,Yv));
}
pixelcvt_end;

//
//twiddled 
//
// 16-bit pixel buffer
pixelcvt_start(conv565_TW,2,2)
{
   //convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB565(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB565(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB565(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB565(p_in[3]));
}
pixelcvt_next(conv1555_TW,2,2)
{
   //convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB1555(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB1555(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB1555(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB1555(p_in[3]));
}
pixelcvt_next(conv4444_TW,2,2)
{
   //convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB4444(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB4444(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB4444(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB4444(p_in[3]));
}

pixelcvt_next(convBMP_TW,2,2)
{
   u16* p_in=(u16*)data;
	pb->prel(0,0,ARGB4444(p_in[0]));
	pb->prel(0,1,ARGB4444(p_in[1]));
	pb->prel(1,0,ARGB4444(p_in[2]));
	pb->prel(1,1,ARGB4444(p_in[3]));
}
pixelcvt_end;

// 32-bit pixel buffer
pixelcvt32_start(conv565_TW32,2,2)
{
	//convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB565_32(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB565_32(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB565_32(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB565_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv1555_TW32,2,2)
{
	//convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB1555_32(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB1555_32(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB1555_32(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB1555_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv4444_TW32,2,2)
{
	//convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB4444_32(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB4444_32(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB4444_32(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB4444_32(p_in[3]));
}
pixelcvt_end;


pixelcvt32_start(convYUV_TW,2,2)
{
   //convert 4x1 4444 to 4x1 8888
	u16* p_in=(u16*)data;


	s32 Y0 = (p_in[0]>>8) &255; //
	s32 Yu = (p_in[0]>>0) &255; //p_in[0]
	s32 Y1 = (p_in[2]>>8) &255; //p_in[3]
	s32 Yv = (p_in[2]>>0) &255; //p_in[2]

	//0,0
	pb->prel(0,0,YUV422<PixelPacker>(Y0,Yu,Yv));
	//1,0
	pb->prel(1,0,YUV422<PixelPacker>(Y1,Yu,Yv));

	//next 4 bytes
	//p_in+=2;

	Y0 = (p_in[1]>>8) &255; //
	Yu = (p_in[1]>>0) &255; //p_in[0]
	Y1 = (p_in[3]>>8) &255; //p_in[3]
	Yv = (p_in[3]>>0) &255; //p_in[2]

	//0,1
	pb->prel(0,1,YUV422<PixelPacker>(Y0,Yu,Yv));
	//1,1
	pb->prel(1,1,YUV422<PixelPacker>(Y1,Yu,Yv));
}
pixelcvt_end;

// 16-bit && 32-bit pixel buffers
pixelcvt_size_start(convPAL4_TW,4,4)
{
   	u8* p_in=(u8*)data;
   u32* pal= sizeof(pixel_size) == 2 ? &palette16_ram[palette_index] : &palette32_ram[palette_index];

	pb->prel(0,0,pal[p_in[0]&0xF]);
	pb->prel(0,1,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(1,0,pal[p_in[0]&0xF]);
	pb->prel(1,1,pal[(p_in[0]>>4)&0xF]);p_in++;

	pb->prel(0,2,pal[p_in[0]&0xF]);
	pb->prel(0,3,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(1,2,pal[p_in[0]&0xF]);
	pb->prel(1,3,pal[(p_in[0]>>4)&0xF]);p_in++;

	pb->prel(2,0,pal[p_in[0]&0xF]);
	pb->prel(2,1,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(3,0,pal[p_in[0]&0xF]);
	pb->prel(3,1,pal[(p_in[0]>>4)&0xF]);p_in++;

	pb->prel(2,2,pal[p_in[0]&0xF]);
	pb->prel(2,3,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(3,2,pal[p_in[0]&0xF]);
	pb->prel(3,3,pal[(p_in[0]>>4)&0xF]);p_in++;
}
pixelcvt_end;

pixelcvt_size_start(convPAL8_TW,2,4)
{
   u8* p_in=(u8*)data;
   u32* pal= sizeof(pixel_size) == 2 ? &palette16_ram[palette_index] : &palette32_ram[palette_index];

	pb->prel(0,0,pal[p_in[0]]);p_in++;
	pb->prel(0,1,pal[p_in[0]]);p_in++;
	pb->prel(1,0,pal[p_in[0]]);p_in++;
	pb->prel(1,1,pal[p_in[0]]);p_in++;

	pb->prel(0,2,pal[p_in[0]]);p_in++;
	pb->prel(0,3,pal[p_in[0]]);p_in++;
	pb->prel(1,2,pal[p_in[0]]);p_in++;
	pb->prel(1,3,pal[p_in[0]]);p_in++;
}
pixelcvt_end;

//handler functions
template<class PixelConvertor, class pixel_type>
void texture_PL(PixelBuffer<pixel_type>* pb,u8* p_in,u32 Width,u32 Height)
{
   pb->amove(0,0);

	Height/=PixelConvertor::ypp;
	Width/=PixelConvertor::xpp;

	for (u32 y=0;y<Height;y++)
	{
		for (u32 x=0;x<Width;x++)
		{
			u8* p = p_in;
			PixelConvertor::Convert(pb,p);
			p_in+=8;

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<class PixelConvertor, class pixel_type>
void texture_TW(PixelBuffer<pixel_type>* pb,u8* p_in,u32 Width,u32 Height)
{
   pb->amove(0,0);

   const u32 divider=PixelConvertor::xpp*PixelConvertor::ypp;

   unsigned long bcx_,bcy_;
   bcx_=bitscanrev(Width);
   bcy_=bitscanrev(Height);
   const u32 bcx=bcx_-3;
   const u32 bcy=bcy_-3;

   for (u32 y=0;y<Height;y+=PixelConvertor::ypp)
   {
      for (u32 x=0;x<Width;x+=PixelConvertor::xpp)
      {
         u8* p = &p_in[(twop(x,y,bcx,bcy)/divider)<<3];
         PixelConvertor::Convert(pb,p);

         pb->rmovex(PixelConvertor::xpp);
      }
      pb->rmovey(PixelConvertor::ypp);
   }
}

template<class PixelConvertor, class pixel_type>
void texture_VQ(PixelBuffer<pixel_type>* pb,u8* p_in,u32 Width,u32 Height)
{
   p_in+=256*4*2;
	pb->amove(0,0);

	const u32 divider=PixelConvertor::xpp*PixelConvertor::ypp;
	unsigned long bcx_,bcy_;
	bcx_=bitscanrev(Width);
	bcy_=bitscanrev(Height);
	const u32 bcx=bcx_-3;
	const u32 bcy=bcy_-3;

	for (u32 y=0;y<Height;y+=PixelConvertor::ypp)
	{
		for (u32 x=0;x<Width;x+=PixelConvertor::xpp)
		{
			u8 p = p_in[twop(x,y,bcx,bcy)/divider];
			PixelConvertor::Convert(pb,&vq_codebook[p*8]);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

//We ask the compiler to generate the templates here
//;)
//planar formats !
template void texture_PL<conv565_PL<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<conv1555_PL<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<conv4444_PL<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<convYUV_PL<pp_8888>, u32>(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<convBMP_PL<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);

#define tex565_PL32 texture_PL<conv565_PL32<pp_8888>, u32>
#define tex1555_PL32 texture_PL<conv1555_PL32<pp_8888>, u32>
#define tex4444_PL32 texture_PL<conv4444_PL32<pp_8888>, u32>

//twiddled formats !
template void texture_TW<conv565_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<conv1555_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<conv4444_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convYUV_TW<pp_8888>, u32>(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convBMP_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);

template void texture_TW<convPAL4_TW<pp_565, u16>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convPAL8_TW<pp_565, u16>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convPAL4_TW<pp_8888, u32>, u32>(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convPAL8_TW<pp_8888, u32>, u32>(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);

//VQ formats !
template void texture_VQ<conv565_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<conv1555_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<conv4444_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<convYUV_TW<pp_8888>, u32>(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<convBMP_TW<pp_565>, u16>(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);

//Planar
#define tex565_PL texture_PL<conv565_PL<pp_565>, u16>
#define tex1555_PL texture_PL<conv1555_PL<pp_565>, u16>
#define tex4444_PL texture_PL<conv4444_PL<pp_565>, u16>
#define texYUV422_PL texture_PL<convYUV_PL<pp_8888>, u32>
#define texBMP_PL texture_PL<convBMP_PL<pp_565>, u16>

//Twiddle
#define tex565_TW texture_TW<conv565_TW<pp_565>, u16>
#define tex1555_TW texture_TW<conv1555_TW<pp_565>, u16>
#define tex4444_TW texture_TW<conv4444_TW<pp_565>, u16>
#define texYUV422_TW texture_TW<convYUV_TW<pp_8888>, u32>
#define texBMP_TW texture_TW<convBMP_TW<pp_565>, u16>
#define texPAL4_TW texture_TW<convPAL4_TW<pp_565, u16>, u16>
#define texPAL8_TW  texture_TW<convPAL8_TW<pp_565, u16>, u16>
#define texPAL4_TW32 texture_TW<convPAL4_TW<pp_8888, u32>, u32>
#define texPAL8_TW32  texture_TW<convPAL8_TW<pp_8888, u32>, u32>

#define tex565_TW32 texture_TW<conv565_TW32<pp_8888>, u32>
#define tex1555_TW32 texture_TW<conv1555_TW32<pp_8888>, u32>
#define tex4444_TW32 texture_TW<conv4444_TW32<pp_8888>, u32>

//VQ
#define tex565_VQ texture_VQ<conv565_TW<pp_565>, u16>
#define tex1555_VQ texture_VQ<conv1555_TW<pp_565>, u16>
#define tex4444_VQ texture_VQ<conv4444_TW<pp_565>, u16>
#define texYUV422_VQ texture_VQ<convYUV_TW<pp_8888>, u32>
#define texBMP_VQ texture_VQ<convBMP_TW<pp_565>, u16>
// According to the documentation, a texture cannot be compressed and use
// a palette at the same time. However the hardware displays them
// just fine.
#define texPAL4_VQ texture_VQ<convPAL4_TW<pp_565, u16>, u16>
#define texPAL8_VQ texture_VQ<convPAL8_TW<pp_565, u16>, u16>

#define tex565_VQ32 texture_VQ<conv565_TW32<pp_8888>, u32>
#define tex1555_VQ32 texture_VQ<conv1555_TW32<pp_8888>, u32>
#define tex4444_VQ32 texture_VQ<conv4444_TW32<pp_8888>, u32>
#define texPAL4_VQ32 texture_VQ<convPAL4_TW<pp_8888, u32>, u32>
#define texPAL8_VQ32 texture_VQ<convPAL8_TW<pp_8888, u32>, u32>

bool VramLockedWriteOffset(size_t offset);
#ifdef HAVE_TEXUPSCALE
void DePosterize(u32* source, u32* dest, int width, int height);
void UpscalexBRZ(int factor, u32* source, u32* dest, int width, int height, bool has_alpha);
#endif

struct PvrTexInfo;
template <class pixel_type> class PixelBuffer;
typedef void TexConvFP(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
typedef void TexConvFP32(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);
enum class TextureType { _565, _5551, _4444, _8888, _8 };

struct BaseTextureCacheData
{
	TSP tsp;        //dreamcast texture parameters
	TCW tcw;

	// Decoded/filtered texture format
	TextureType tex_type;

	u32 Lookups;

	u32 sa;         //pixel data start address in vram (might be offset for mipmaps/etc)
	u32 sa_tex;		//texture data start address in vram
	u32 w,h;        //width & height of the texture
	u32 size;       //size, in bytes, in vram

	const PvrTexInfo* tex;
	TexConvFP*  texconv;
	TexConvFP32*  texconv32;

	u32 dirty;
	vram_block* lock_block;

	u32 Updates;

	u32 palette_index;
	//used for palette updates
	u32 palette_hash;			// Palette hash at time of last update
	u32 vq_codebook;            // VQ quantizers table for compressed textures
	u32 texture_hash;			// xxhash of texture data, used for custom textures
	u32 old_texture_hash;		// legacy hash
	u8* custom_image_data;		// loaded custom image data
	u32 custom_width;
	u32 custom_height;
	std::atomic_int custom_load_in_progress;

	void PrintTextureName();
	virtual std::string GetId() = 0;

	bool IsPaletted()
	{
		return tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8;
	}

	const char* GetPixelFormatName()
	{
		switch (tcw.PixelFmt)
		{
		case Pixel1555: return "1555";
		case Pixel565: return "565";
		case Pixel4444: return "4444";
		case PixelYUV: return "yuv";
		case PixelBumpMap: return "bumpmap";
		case PixelPal4: return "pal4";
		case PixelPal8: return "pal8";
		default: return "unknown";
		}
	}

	void Create();
	void ComputeHash();
	void Update();
	virtual void UploadToGPU(int width, int height, u8 *temp_tex_buffer) = 0;
	virtual bool Force32BitTexture(TextureType type) { return false; }
	void CheckCustomTexture();
	//true if : dirty or paletted texture and hashes don't match
	bool NeedsUpdate();
	virtual bool Delete();
	virtual ~BaseTextureCacheData() {}
};

template<typename Texture>
class BaseTextureCache
{
	using TexCacheIter = typename std::unordered_map<u64, Texture>::iterator;
public:
	Texture *getTextureCacheData(TSP tsp, TCW tcw)
	{
		u64 key = tsp.full & TSPTextureCacheMask.full;
		if (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
			// Paletted textures have a palette selection that must be part of the key
			// We also add the palette type to the key to avoid thrashing the cache
			// when the palette type is changed. If the palette type is changed back in the future,
			// this texture will stil be available.
			key |= ((u64)tcw.full << 32) | ((PAL_RAM_CTRL & 3) << 6);
		else
			key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

		TexCacheIter it = cache.find(key);

		Texture* texture;
		if (it != cache.end())
		{
			texture = &it->second;
			// Needed if the texture is updated
			texture->tcw.StrideSel = tcw.StrideSel;
		}
		else //create if not existing
		{
			texture = &cache[key];

			texture->tsp = tsp;
			texture->tcw = tcw;
		}
		texture->Lookups++;

		return texture;
	}

	void CollectCleanup()
	{
		vector<u64> list;

		u32 TargetFrame = max((u32)120, FrameCount) - 120;

		for (const auto& pair : cache)
		{
			if (pair.second.dirty && pair.second.dirty < TargetFrame)
				list.push_back(pair.first);

			if (list.size() > 5)
				break;
		}

		for (u64 id : list)
		{
			if (cache[id].Delete())
				cache.erase(id);
		}
	}

	void Clear()
	{
		for (auto& pair : cache)
			pair.second.Delete();

		cache.clear();
		KillTex = false;
		INFO_LOG(RENDERER, "Texture cache cleared");
	}

private:
	std::unordered_map<u64, Texture> cache;
	// Only use TexU and TexV from TSP in the cache key
	//     TexV : 7, TexU : 7
	const TSP TSPTextureCacheMask = { { 7, 7 } };
	//     TexAddr : 0x1FFFFF, Reserved : 0, StrideSel : 0, ScanOrder : 1, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
	const TCW TCWTextureCacheMask = { { 0x1FFFFF, 0, 0, 1, 7, 1, 1 } };
};

void rend_text_invl(vram_block* bl);

void ReadFramebuffer(PixelBuffer<u32>& pb, int& width, int& height);
void WriteTextureToVRam(u32 width, u32 height, u8 *data, u16 *dst);

static inline void MakeFogTexture(u8 *tex_data)
{
	u8 *fog_table = (u8 *)FOG_TABLE;
	for (int i = 0; i < 128; i++)
	{
		tex_data[i] = fog_table[i * 4];
		tex_data[i + 128] = fog_table[i * 4 + 1];
	}
}
