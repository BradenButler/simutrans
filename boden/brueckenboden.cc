
#include "../simdebug.h"

#include "../simimg.h"
#include "../simworld.h"
#include "../simhalt.h"

#include "../besch/grund_besch.h"

#include "../dataobj/loadsave.h"
#include "../dataobj/freelist.h"

#include "../gui/ground_info.h"

#include "brueckenboden.h"

brueckenboden_t::brueckenboden_t(karte_t *welt, loadsave_t *file) : grund_t(welt)
{
	rdwr(file);
}


brueckenboden_t::brueckenboden_t(karte_t *welt, koord3d pos, int grund_hang, int weg_hang) : grund_t(welt, pos)
{
	slope = grund_hang;
	this->weg_hang = weg_hang;
}


void brueckenboden_t::calc_bild()
{
	if(ist_karten_boden()) {
		if(gib_hoehe()==welt->gib_grundwasser()) {
			setze_bild(grund_besch_t::ufer->gib_bild(slope));
		}
		else {
			setze_bild(grund_besch_t::boden->gib_bild(slope));
		}
		grund_t::calc_back_bild(gib_hoehe()/16,slope);
	}
	else {
		clear_back_bild();
		setze_bild(IMG_LEER);
	}
	if(wege[1]) {
		wege[1]->calc_bild(pos);
	}
	for(uint8 i=0;  i<gib_top();  i++  ) {
		ding_t *dt=obj_bei(i);
		if(dt) {
			dt->calc_bild();
		}
	}
	set_flag(draw_as_ding);
}


void
brueckenboden_t::rdwr(loadsave_t *file)
{
	grund_t::rdwr(file);

	if(file->get_version()<88009) {
		uint8 sl;
		file->rdwr_byte( sl, " " );
		slope = sl;
	}
	file->rdwr_byte(weg_hang, "\n");
}


int brueckenboden_t::gib_weg_yoff() const
{
	if(ist_karten_boden() && weg_hang == 0) {
		return 16;
	}
	else {
		return 0;
	}
}


bool
brueckenboden_t::zeige_info()
{
	if(gib_halt().is_bound()) {
		gib_halt()->zeige_info();
		return true;
	}
	else {
		if(hat_wege()) {	// if this is true, then all land info is shown
			// there is some info!
			if(!grund_infos->get(this)) {
				grund_infos->put(this, new grund_info_t(welt, this));
			}
			create_win(-1, -1, grund_infos->get(this), w_autodelete);
			return true;
		}
	}
	return false;
}


void * brueckenboden_t::operator new(size_t /*s*/)
{
	return (brueckenboden_t *)freelist_t::gimme_node(sizeof(brueckenboden_t));
}


void brueckenboden_t::operator delete(void *p)
{
	freelist_t::putback_node(sizeof(brueckenboden_t),p);
}
