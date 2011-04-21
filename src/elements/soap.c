#include <element.h>

void detach(int i)
{
	if ((parts[i].ctype&2) == 2)
	{
		if ((parts[parts[i].tmp].ctype&4) == 4)
			parts[parts[i].tmp].ctype ^= 4;
	}

	if ((parts[i].ctype&4) == 4)
	{
		if ((parts[parts[i].tmp2].ctype&2) == 2)
			parts[parts[i].tmp2].ctype ^= 2;
	}

	parts[i].ctype = 0;
}

int update_SOAP(UPDATE_FUNC_ARGS) 
{
	int r, rx, ry;

	//0x01 - bubble on/off
	//0x02 - first mate yes/no
	//0x04 - "back" mate yes/no

	if ((parts[i].ctype&1) == 1)
	{
		if (parts[i].life<=0)
		{
			if ((parts[i].ctype&6) == 0)
				parts[i].ctype = 0;

			if ((parts[i].ctype&6) != 6 && parts[i].ctype>1)
			{
				int target;

				target = i;

				while((parts[target].ctype&6) != 6 && parts[target].ctype>1)
				{
					if ((parts[target].ctype&2) == 2)
					{
						target = parts[target].tmp;
						detach(target);
					}

					if ((parts[target].ctype&4) == 4)
					{
						target = parts[target].tmp2;
						detach(target);
					}
				}
			}

			if ((parts[i].ctype&6) == 6 && (parts[parts[i].tmp].ctype&6) == 6 && parts[parts[i].tmp].tmp == i)
				detach(i);

		}

		parts[i].vy -= 0.1f;

		parts[i].vy *= 0.5f;
		parts[i].vx *= 0.5f;

		if((parts[i].ctype&2) != 2)
		{
			for (rx=-2; rx<3; rx++)
				for (ry=-2; ry<3; ry++)
					if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
					{
						r = pmap[y+ry][x+rx];
						if ((r>>8)>=NPART || !r)
							continue;

						if ((r&0xFF) != PT_SOAP || (parts[r>>8].ctype == 0 && (r&0xFF) == PT_SOAP))
						{
							detach(i);
							continue;
						}

						if ((parts[r>>8].type == PT_SOAP) && ((parts[r>>8].ctype&1) == 1) 
								&& ((parts[r>>8].ctype&4) != 4))
						{
							if ((parts[r>>8].ctype&2) == 2)
							{
								parts[i].tmp = r>>8;
								parts[r>>8].tmp2 = i;

								parts[i].ctype |= 2;
								parts[r>>8].ctype |= 4;
							}
							else
							{
								if ((parts[i].ctype&2) != 2)
								{
									parts[i].tmp = r>>8;
									parts[r>>8].tmp2 = i;

									parts[i].ctype |= 2;
									parts[r>>8].ctype |= 4;
								}
							}
						}
					}
		}

		if((parts[i].ctype&2) == 2)
		{
			float d, dx, dy;

			dx = parts[i].x - parts[parts[i].tmp].x;
			dy = parts[i].y - parts[parts[i].tmp].y;

			d = 9/(pow(dx, 2)+pow(dy, 2)+9)-0.5;

			parts[parts[i].tmp].vx -= dx*d;
			parts[parts[i].tmp].vy -= dy*d;

			parts[i].vx += dx*d;
			parts[i].vy += dy*d;

			if (((parts[parts[i].tmp].ctype&2) == 2) && ((parts[parts[i].tmp].ctype&1) == 1) 
					&& ((parts[parts[parts[i].tmp].tmp].ctype&2) == 2) && ((parts[parts[parts[i].tmp].tmp].ctype&1) == 1))
			{
				int ii;

				ii = parts[parts[parts[i].tmp].tmp].tmp;

				dx = parts[ii].x - parts[parts[i].tmp].x;
				dy = parts[ii].y - parts[parts[i].tmp].y;

				d = 81/(pow(dx, 2)+pow(dy, 2)+81)-0.5;

				parts[parts[i].tmp].vx -= dx*d*0.5f;
				parts[parts[i].tmp].vy -= dy*d*0.5f;

				parts[ii].vx += dx*d*0.5f;
				parts[ii].vy += dy*d*0.5f;
			}
		}
	}
	else
	{
		if (pv[y/CELL][x/CELL]>0.5f || pv[y/CELL][x/CELL]<(-0.5f))
		{
			parts[i].ctype = 1;
			parts[i].life = 5;
		}
	}

	return 0;
}