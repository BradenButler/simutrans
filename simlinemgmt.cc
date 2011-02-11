/*
 * part of the Simutrans project
 * @author hsiegeln
 * 01/12/2003
 */

#include <algorithm>
#include "simlinemgmt.h"
#include "simline.h"
#include "simhalt.h"
#include "simwin.h"
#include "simworld.h"
#include "simtypes.h"
#include "simintr.h"

#include "dataobj/fahrplan.h"
#include "dataobj/loadsave.h"

#include "gui/schedule_list.h"

#include "player/simplay.h"

uint8 simlinemgmt_t::used_ids[8192];
uint16 simlinemgmt_t::last_id = 0;
karte_t *simlinemgmt_t::welt = NULL;

void simlinemgmt_t::init_line_ids()
{
	DBG_MESSAGE("simlinemgmt_t::init_line_ids()","done");
	for(int i=0;  i<8192;  i++  ) {
		used_ids[i] = 0;
	}
	used_ids[INVALID_LINE_ID/8] |= (1<<(INVALID_LINE_ID&7));
	used_ids[REASSIGN_LINE_ID/8] |= (1<<(REASSIGN_LINE_ID&7));
	last_id = 0;
}


simlinemgmt_t::simlinemgmt_t(karte_t* welt)
{
	this->welt = welt;
	schedule_list_gui = NULL;
}


simlinemgmt_t::~simlinemgmt_t()
{
	destroy_win( (long)this );
	// and delete all lines ...
	while(  all_managed_lines.get_count()>0  ) {
		linehandle_t line = all_managed_lines.back();
		all_managed_lines.remove_at( all_managed_lines.get_count()-1 );
		delete line.get_rep();	// detaching handled by line itself
	}
}


void simlinemgmt_t::line_management_window(spieler_t *sp)
{
	schedule_list_gui = dynamic_cast<schedule_list_gui_t *>(win_get_magic( magic_line_management_t+sp->get_player_nr() ));
	if(  schedule_list_gui==NULL  ) {
		schedule_list_gui = new schedule_list_gui_t(sp);
		create_win( schedule_list_gui, w_info, magic_line_management_t+sp->get_player_nr() );
	}
}


void simlinemgmt_t::add_line(linehandle_t new_line)
{
	uint16 id = new_line->get_line_id();
DBG_MESSAGE("simlinemgmt_t::add_line()","id=%d",new_line->get_line_id());
	if(  id!=INVALID_LINE_ID  ) {
		// update last line ID to the largest ID number
		if(  last_id<id  ) {
			last_id = id;
		}
		// case : line restored from save game
		if(  ( used_ids[id/8] & (1<<(id&7)) )!=0  ) {
			// line ID is already used -> defer reassignment of line ID to check_create_id() after all lines are loaded
			dbg->error("simlinemgmt_t::add_line()","Line id %i doubled! (0x%X)!", id, used_ids[id/8] );
			new_line->set_line_id( REASSIGN_LINE_ID );
		}
		else {
			// register line ID as used
			used_ids[id/8] |= (1<<(id&7));
		}
	}
	all_managed_lines.append(new_line);
}


// since after loading there might be double line ID this routine will correct it
void simlinemgmt_t::check_create_id(linehandle_t new_line)
{
	uint16 id = new_line->get_line_id();
	if(  id==INVALID_LINE_ID  ) {
		id = get_unique_line_id();
		new_line->set_line_id( id );
		DBG_MESSAGE("simlinemgmt_t::check_create_id()","New line id %i", id );
	}
	else if(  id==REASSIGN_LINE_ID  ) {
		id = get_unique_line_id();
		new_line->set_line_id( id );
		dbg->error("simlinemgmt_t::check_create_id()","Reassigned new line id %i to line %s!", id, new_line->get_name());
	}
}


linehandle_t simlinemgmt_t::get_line_by_id(uint16 id)
{
	for (vector_tpl<linehandle_t>::const_iterator i = all_managed_lines.begin(), end = all_managed_lines.end(); i != end; i++) {
		if ((*i)->get_line_id() == id) return *i;
	}
	dbg->warning("simlinemgmt_t::get_line_by_id()","no line for id=%i",id);
	return linehandle_t();
}


void simlinemgmt_t::delete_line(linehandle_t line)
{
	if (line.is_bound()) {
		// free the line ID
		uint16 id = line->get_line_id();
		used_ids[id/8] &= ~(1<<(id&7));
		// if possible, rewind the last line ID
		while(  ( used_ids[last_id/8] & (1<<(last_id&7)) )==0  ) {
			--last_id;
		}
		all_managed_lines.remove(line);
		//destroy line object
		delete line.get_rep();
	}
}


void simlinemgmt_t::update_line(linehandle_t line)
{
	// when a line is updated, all managed convoys must get the new fahrplan!
	int count = line->count_convoys();
	for(int i = 0; i<count; i++) {
		line->get_convoy(i)->set_update_line(line);
	}
	// finally de/register all stops
	line->renew_stops();
	if(  count>0  ) {
		welt->set_schedule_counter();
	}
}



void simlinemgmt_t::rdwr(karte_t * welt, loadsave_t *file, spieler_t *sp)
{
	xml_tag_t l( file, "simlinemgmt_t" );

	if(file->is_saving()) {
		// on old versions
		if(  file->get_version()<101000  ) {
			file->wr_obj_id("Linemanagement");
		}

		// ensure that lines are saved in the same order on all clients
		sort_lines();

		uint32 count = all_managed_lines.get_count();
		file->rdwr_long(count);
		for (vector_tpl<linehandle_t>::const_iterator i = all_managed_lines.begin(), end = all_managed_lines.end(); i != end; i++) {
			simline_t::linetype lt = (*i)->get_linetype();
			file->rdwr_enum(lt);
			(*i)->rdwr(file);
		}
	}
	else {
		// on old versions
		if(  file->get_version()<101000  ) {
			char buf[80];
			file->rd_obj_id(buf, 79);
			all_managed_lines.clear();
			if(strcmp(buf, "Linemanagement")!=0) {
				dbg->fatal( "simlinemgmt_t::rdwr", "Broken Savegame" );
			}
		}
		sint32 totalLines = 0;
		file->rdwr_long(totalLines);
DBG_MESSAGE("simlinemgmt_t::rdwr()","number of lines=%i",totalLines);
		for (int i = 0; i<totalLines; i++) {
			simline_t::linetype lt=simline_t::line;
			file->rdwr_enum(lt);
			simline_t * line;
			switch(lt) {
				case simline_t::truckline:    line = new truckline_t(   welt, sp); break;
				case simline_t::trainline:    line = new trainline_t(   welt, sp); break;
				case simline_t::shipline:     line = new shipline_t(    welt, sp); break;
				case simline_t::airline:      line = new airline_t(     welt, sp); break;
				case simline_t::monorailline: line = new monorailline_t(welt, sp); break;
				case simline_t::tramline:     line = new tramline_t(    welt, sp); break;
				case simline_t::maglevline:   line = new maglevline_t(  welt, sp); break;
				case simline_t::narrowgaugeline:line = new narrowgaugeline_t(welt, sp); break;
				default:
					// line = new simline_t(     welt, sp); break;
					dbg->fatal( "simlinemgmt_t::create_line()", "Cannot create default line!" );
			}
			line->rdwr(file);
			add_line( line->get_handle() );
		}
	}
}


static bool compare_lines(const linehandle_t& a, const linehandle_t& b)
{
	int diff = 0;
	if(  a->get_name()[0]=='('  &&  b->get_name()[0]=='('  ) {
		diff = atoi(a->get_name()+1)-atoi(b->get_name()+1);
	}
	if(  diff==0  ) {
		diff = strcmp(a->get_name(), b->get_name());
	}
	if(diff==0) {
		diff = a.get_id() - b.get_id();
	}
	return diff < 0;
}


void simlinemgmt_t::sort_lines()
{
	std::sort(all_managed_lines.begin(), all_managed_lines.end(), compare_lines);
}


void simlinemgmt_t::laden_abschliessen()
{
	sort_lines();
	for (vector_tpl<linehandle_t>::const_iterator i = all_managed_lines.begin(), end = all_managed_lines.end(); i != end; i++) {
		check_create_id( *i );
		(*i)->laden_abschliessen();
	}
	sort_lines();
}


void simlinemgmt_t::rotate90( sint16 y_size )
{
	for (vector_tpl<linehandle_t>::const_iterator i = all_managed_lines.begin(), end = all_managed_lines.end(); i != end; i++) {
		schedule_t *fpl = (*i)->get_schedule();
		if(fpl) {
			fpl->rotate90( y_size );
		}
	}
}



/**
 * Creates a unique line id. (max uint16, but this should be enough anyway)
 * @author prissi
 */
uint16 simlinemgmt_t::get_unique_line_id()
{
	++last_id;
DBG_MESSAGE("simlinemgmt_t::get_unique_line_id()", "Free ID near %u", last_id);
	// first, try to find an ID larger than the last ID
	const uint16 last_id_i = last_id / 8;
	const uint16 last_id_j = last_id & 7;
	uint16 j = last_id_j;
	for(  uint16 i=last_id_i;  i<8192;  ++i  ) {
		if(  used_ids[i]!=255  ) {
			for(  ;  j<8;  ++j  ) {
				if(  ( used_ids[i]&(1<<j) )==0  ) {
					used_ids[i] |= (1<<j);
					last_id = i * 8 + j;
DBG_MESSAGE("simlinemgmt_t::get_unique_line_id()", "New ID %i", last_id);
					return last_id;
				}
			}
		}
		j = 0;
	}
	// if no larger ID available, try to find an ID smaller than the last ID
	for(  uint16 i=0;  i<=last_id_i;  ++i  ) {
		if(  used_ids[i]!=255  ) {
			for(  j=0;  j<8;  ++j  ) {
				if(  ( used_ids[i]&(1<<j) )==0  ) {
					used_ids[i] |= (1<<j);
					last_id = i * 8 + j;
DBG_MESSAGE("simlinemgmt_t::get_unique_line_id()", "New ID %i", last_id);
					return last_id;
				}
			}
		}
	}
	// not found
	dbg->error("simlinemgmt_t::get_unique_line_id()","No valid ID found!");
	return INVALID_LINE_ID;
}


void simlinemgmt_t::new_month()
{
	for (vector_tpl<linehandle_t>::const_iterator i = all_managed_lines.begin(), end = all_managed_lines.end(); i != end; i++) {
		(*i)->new_month();
	}
}


linehandle_t simlinemgmt_t::create_line(int ltype, spieler_t * sp)
{
	simline_t * line = NULL;
	switch (ltype) {
		case simline_t::truckline:
			line = new truckline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "truckline created");
			break;
		case simline_t::trainline:
			line = new trainline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "trainline created");
			break;
		case simline_t::shipline:
			line = new shipline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "shipline created");
			break;
		case simline_t::airline:
			line = new airline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "airline created");
			break;
		case simline_t::monorailline:
			line = new monorailline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "monorailline created");
			break;
		case simline_t::tramline:
			line = new tramline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "tramline created");
			break;
		case simline_t::maglevline:
			line = new maglevline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "maglevline created");
			break;
		case simline_t::narrowgaugeline:
			line = new narrowgaugeline_t(welt, sp);
			DBG_MESSAGE("simlinemgmt_t::create_line()", "narrowgaugeline created");
			break;
		default:
			dbg->fatal( "simlinemgmt_t::create_line()", "Cannot create default line!" );
			break;
	}
	add_line( line->get_handle() );
	check_create_id( line->get_handle() );
	sort_lines();
	return line->get_handle();
}



linehandle_t simlinemgmt_t::create_line(int ltype, spieler_t * sp, schedule_t * fpl)
{
	linehandle_t line = create_line( ltype, sp );
	if(fpl) {
		line->get_schedule()->copy_from(fpl);
	}
	return line;
}


void simlinemgmt_t::get_lines(int type, vector_tpl<linehandle_t>* lines) const
{
	lines->clear();
	for (vector_tpl<linehandle_t>::const_iterator i = all_managed_lines.begin(), end = all_managed_lines.end(); i != end; i++) {
		linehandle_t line = *i;
		if (type == simline_t::line || line->get_linetype() == simline_t::line || line->get_linetype() == type) {
			lines->append(line);
		}
	}
}

void simlinemgmt_t::show_lineinfo(spieler_t *sp, linehandle_t line)
{
	line_management_window(sp);
	schedule_list_gui->show_lineinfo(line);
}
