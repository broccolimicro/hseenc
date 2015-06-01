/*
 * main.cpp
 *
 *  Created on: Jan 16, 2015
 *      Author: nbingham
 */

#include <common/standard.h>
#include <parse/parse.h>
#include <parse_hse/parallel.h>
#include <hse/graph.h>
#include <hse/simulator.h>
#include <hse/encoder.h>
#include <interpret_hse/import.h>
#include <interpret_hse/export.h>
#include <interpret_dot/export.h>
#include <interpret_dot/import.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <boolean/variable.h>
#include <boolean/factor.h>

void print_help()
{
	printf("Usage: hseenc [options] file...\n");
	printf("A state encoder and conflict solver for HSE\n");
	printf("\nGeneral Options:\n");
	printf(" -h,--help      Display this information\n");
	printf("    --version   Display version information\n");
	printf(" -v,--verbose   Display verbose messages\n");
	printf(" -d,--debug     Display internal debugging messages\n");
	printf("\nConflict Checking:\n");
	printf(" -c             check for state conflicts that occur regardless of sense\n");
	printf(" -cu            check for state conflicts that occur due to up-going transitions\n");
	printf(" -cd            check for state conflicts that occur due to down-going transitions\n");
	printf(" -s             check for potential state conflicts that occur regardless of sense\n");
	printf(" -su            check for potential state conflicts that occur due to up-going transitions\n");
	printf(" -sd            check for potential state conflicts that occur due to down-going transitions\n");
}

void print_version()
{
	printf("hseenc 1.0.0\n");
	printf("Copyright (C) 2013 Sol Union.\n");
	printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
}

void print_command_help()
{
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf("\nGeneral:\n");
	printf(" help, h                       print this message\n");
	printf(" quit, q                       exit the interactive simulation environment\n");
	printf(" load (filename)               load an hse, default is to reload current file\n");
	printf(" save (filename)               save an hse, default is to overwrite current file\n");
	printf("\nViewing and Manipulating State:\n");
	printf(" elaborate, e                  elaborate the predicates\n");
	printf(" conflicts, c                  check for state conflicts that occur regardless of sense\n");
	printf(" conflicts up, cu              check for state conflicts that occur due to up-going transitions\n");
	printf(" conflicts down, cd            check for state conflicts that occur due to down-going transitions\n");
	printf(" suspects, s                   check for potential state conflicts that occur regardless of sense\n");
	printf(" suspects up, su               check for potential state conflicts that occur due to up-going transitions\n");
	printf(" suspects down, sd             check for potential state conflicts that occur due to down-going transitions\n");
	printf("\nViewing and Manipulating Structure:\n");
	printf(" print, p                      print the current hse\n");
	printf(" insert <expr>                 insert the transition <expr> into the hse\n");
	printf(" pinch <node>                  remove <node> and merge its input and output nodes to maintain token flow\n");
}

void print_location_help()
{
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf(" help                          print this message\n");
	printf(" done                          exit the interactive location selector\n");
	printf(" before <i>, b<i>              put the state variable before the selected hse block\n");
	printf(" after <i>, a<i>               put the state variable after the selected hse block\n");
	printf(" <i>                           go into the selected hse block\n");
	printf(" up, u                         back out of the current hse block\n");
}

bool find(vector<hse::iterator> n, parse::syntax *syn)
{
	if (syn->is_a<parse_boolean::disjunction>())
		return find(n.begin(), n.end(), hse::iterator(hse::transition::type, syn->start)) != n.end();
	else if (syn->is_a<parse_boolean::internal_choice>())
		return find(n.begin(), n.end(), hse::iterator(hse::transition::type, syn->start)) != n.end();
	else if (syn->is_a<parse_hse::sequence>())
	{
		parse_hse::sequence *s = (parse_hse::sequence*)syn;
		bool found = false;
		for (int i = 0; i < s->actions.size(); i++)
			found = found || find(n, s->actions[i]);
		return found;
	}
	else if (syn->is_a<parse_hse::parallel>())
	{
		parse_hse::parallel *s = (parse_hse::parallel*)syn;
		bool found = false;
		for (int i = 0; i < s->branches.size(); i++)
			found = found || find(n, &s->branches[i]);
		return found;
	}
	else if (syn->is_a<parse_hse::condition>())
	{
		parse_hse::condition *s = (parse_hse::condition*)syn;
		bool found = false;
		for (int i = 0; i < s->branches.size(); i++)
			found = found || find(n, &s->branches[i].first) || find(n, &s->branches[i].second);
		return found;
	}
	else if (syn->is_a<parse_hse::loop>())
	{
		parse_hse::loop *s = (parse_hse::loop*)syn;
		bool found = false;
		for (int i = 0; i < s->branches.size(); i++)
			found = found || find(n, &s->branches[i].first) || find(n, &s->branches[i].second);
		return found;
	}
	else
		return false;
}

string place_to_string(parse::syntax *syn, vector<hse::iterator> p, vector<hse::iterator> n, bool loop)
{
	if (syn->is_a<parse_hse::sequence>())
	{
		parse_hse::sequence *s = (parse_hse::sequence*)syn;
		for (int i = 0; i < s->actions.size(); i++)
		{
			if (i != 0 && find(p, s->actions[i-1]) && find(n, s->actions[i]))
				return s->actions[i-1]->to_string() + " ; " + s->actions[i]->to_string();
			else if (find(p, s->actions[i]) && find(n, s->actions[i]))
				return place_to_string(s->actions[i], p, n, false);
		}

		if (loop && s->actions.size() > 0 && find(p, s->actions.back()) && find(n, s->actions[0]))
			return s->actions.back()->to_string() + " ; " + s->actions[0]->to_string();

		return "not found";
	}
	else if (syn->is_a<parse_hse::parallel>())
	{
		parse_hse::parallel *s = (parse_hse::parallel*)syn;
		for (int i = 0; i < s->branches.size(); i++)
			if (find(p, &s->branches[i]) && find(n, &s->branches[i]))
				return place_to_string(&s->branches[i], p, n, loop);
		return "not found";
	}
	else if (syn->is_a<parse_hse::condition>())
	{
		parse_hse::condition *s = (parse_hse::condition*)syn;
		for (int i = 0; i < s->branches.size(); i++)
		{
			if (find(p, &s->branches[i].first) && find(n, &s->branches[i].second))
				return s->branches[i].first.to_string() + " -> " + s->branches[i].second.to_string();
			else if (find(p, &s->branches[i].second) && find(n, &s->branches[i].second))
				return place_to_string(&s->branches[i].second, p, n, loop);
		}
		return "not found";
	}
	else if (syn->is_a<parse_hse::loop>() && find(p, syn) && find(n, syn))
	{
		parse_hse::loop *s = (parse_hse::loop*)syn;
		for (int i = 0; i < s->branches.size(); i++)
		{
			if (find(p, &s->branches[i].first) && find(n, &s->branches[i].second))
				return s->branches[i].first.to_string() + " -> " + s->branches[i].second.to_string();
			else if (find(n, &s->branches[i].first) && find(p, &s->branches[i].second))
				return s->branches[i].second.to_string() + " ; " + s->branches[i].first.to_string();
			else if (find(p, &s->branches[i].second) && find(n, &s->branches[i].second))
				return place_to_string(&s->branches[i].second, p, n, true);
		}
		return "not found";
	}
	else
		return "not found";
}

string node2string(hse::iterator i, hse::graph &g, boolean::variable_set &v)
{
	vector<hse::iterator> n = g.next(i);
	vector<hse::iterator> p = g.prev(i);
	string result = "";

	if (i.type == hse::transition::type)
	{
		vector<hse::iterator> pp;
		vector<hse::iterator> np;

		bool proper_nest = true;
		for (int j = 0; j < (int)p.size(); j++)
		{
			vector<hse::iterator> tmp = g.prev(p[j]);
			pp.insert(pp.begin(), tmp.begin(), tmp.end());
			tmp = g.next(p[j]);
			np.insert(np.begin(), tmp.begin(), tmp.end());
			if (p.size() > 1 && tmp.size() > 1)
				proper_nest = false;
		}

		sort(pp.begin(), pp.end());
		pp.resize(unique(pp.begin(), pp.end()) - pp.begin());
		sort(np.begin(), np.end());
		np.resize(unique(np.begin(), np.end()) - np.begin());

		n = np;
		p = pp;
	}

	if (p.size() > 1)
	{
		result = "[...";
		for (int j = 0; j < (int)p.size(); j++)
		{
			if (j != 0)
				result += "[]...";

			if (g.transitions[p[j].index].behavior == hse::transition::active)
				result += export_internal_choice(g.transitions[p[j].index].action, v).to_string();
			else
				result += "[" + export_disjunction(g.transitions[p[j].index].action, v).to_string() + "]";
		}
		result += "] ; ";
	}
	else if (p.size() == 1 && g.transitions[p[0].index].behavior == hse::transition::active)
		result =  export_internal_choice(g.transitions[p[0].index].action, v).to_string() + " ; ";
	else if (p.size() == 1 && g.next(g.prev(p[0])).size() > 1)
		result = "[" + export_disjunction(g.transitions[p[0].index].action, v).to_string() + " -> ";
	else if (p.size() == 1)
		result = "[" + export_disjunction(g.transitions[p[0].index].action, v).to_string() + "] ; ";

	if (n.size() > 1)
	{
		result += "[";
		for (int j = 0; j < (int)n.size(); j++)
		{
			if (j != 0)
				result += "[]";

			if (n[j] == i)
				result += " ";

			if (g.transitions[n[j].index].behavior == hse::transition::active)
				result += "1->" + export_internal_choice(g.transitions[n[j].index].action, v).to_string() + "...";
			else
				result += export_disjunction(g.transitions[n[j].index].action, v).to_string() + "->...";

			if (n[j] == i)
				result += " ";
		}
		result += "]";
	}
	else if (n.size() == 1 && g.transitions[n[0].index].behavior == hse::transition::active)
		result += export_internal_choice(g.transitions[n[0].index].action, v).to_string();
	else if (n.size() == 1 && g.prev(g.next(n[0])).size() > 1)
		result += export_disjunction(g.transitions[n[0].index].action, v).to_string() + "]";
	else if (n.size() == 1)
		result += "[" + export_disjunction(g.transitions[n[0].index].action, v).to_string() + "]";

	return result;
}

void print_conflicts(hse::encoder &enc, hse::graph &g, boolean::variable_set &v, int sense)
{
	for (int i = 0; i < (int)enc.conflicts.size(); i++)
	{
		if (enc.conflicts[i].sense == sense)
		{
			vector<hse::iterator> imp;
			boolean::cover implicant = 1;
			for (int j = 0; j < (int)enc.conflicts[i].implicant.size(); j++)
			{
				imp.push_back(hse::iterator(hse::place::type, enc.conflicts[i].implicant[j]));
				implicant &= g.places[enc.conflicts[i].implicant[j]].effective;
			}
			printf("T%d.%d\t%s\n{\n", enc.conflicts[i].index.index, enc.conflicts[i].index.term, node2string(hse::iterator(hse::transition::type, enc.conflicts[i].index.index), g, v).c_str());

			implicant = implicant.mask(g.transitions[enc.conflicts[i].index.index].action[enc.conflicts[i].index.term].mask()).mask(sense);

			for (int j = 0; j < (int)enc.conflicts[i].region.size(); j++)
			{
				hse::iterator k(hse::place::type, enc.conflicts[i].region[j]);
				printf("\tP%d\t%s\t%s\n", enc.conflicts[i].region[j], node2string(k, g, v).c_str(), export_disjunction(boolean::factor().hfactor(implicant & g.places[enc.conflicts[i].region[j]].effective), v).to_string().c_str());
			}
			printf("}\n");
		}
	}
	printf("\n");
}

void print_suspects(hse::encoder &enc, hse::graph &g, boolean::variable_set &v, int sense)
{
	for (int i = 0; i < (int)enc.suspects.size(); i++)
	{
		if (enc.suspects[i].sense == sense)
		{
			printf("{\n");
			for (int j = 0; j < (int)enc.suspects[i].first.size(); j++)
			{
				hse::iterator k(hse::place::type, enc.suspects[i].first[j]);
				printf("\tP%d\t%s\n", enc.suspects[i].first[j], node2string(k, g, v).c_str());
			}
			printf("=============================================\n");

			for (int j = 0; j < (int)enc.suspects[i].second.size(); j++)
			{
				hse::iterator k(hse::place::type, enc.suspects[i].second[j]);
				printf("\tP%d\t%s\n", enc.suspects[i].second[j], node2string(k, g, v).c_str());
			}
			printf("}\n");
		}
	}
	printf("\n");
}

vector<pair<hse::iterator, int> > get_locations(FILE *script, hse::graph &g, boolean::variable_set &v)
{
	vector<pair<hse::iterator, int> > result;

	parse_hse::parallel p = export_parallel(g, v);

	vector<parse::syntax*> stack(1, &p);

	bool update = true;

	int n;
	char command[256];
	bool done = false;
	while (!done)
	{
		while (update)
		{
			if (stack.back()->is_a<parse_hse::parallel>())
			{
				parse_hse::parallel *par = (parse_hse::parallel*)stack.back();

				if (par->branches.size() == 1)
					stack.back() = &par->branches[0];
				else
				{
					for (int i = 0; i < par->branches.size(); i++)
						printf("(%d) %s\n", i, par->branches[i].to_string().c_str());
					update = false;
				}
			}
			else if (stack.back()->is_a<parse_hse::condition>())
			{
				parse_hse::condition *par = (parse_hse::condition*)stack.back();
				if (par->branches.size() == 1 && par->branches[0].second.branches.size() == 0)
					stack.back() = &par->branches[0].first;
				else
				{
					for (int i = 0; i < par->branches.size(); i++)
						printf("(%d) %s -> (%d) %s\n", i*2, par->branches[i].first.to_string().c_str(), i*2+1, par->branches[i].second.to_string().c_str());
					update = false;
				}
			}
			else if (stack.back()->is_a<parse_hse::loop>())
			{
				parse_hse::loop *par = (parse_hse::loop*)stack.back();
				if (par->branches.size() == 1 && !par->branches[0].first.valid)
					stack.back() = &par->branches[0].second;
				else
				{
					for (int i = 0; i < par->branches.size(); i++)
						printf("(%d) %s -> (%d) %s\n", i*2, par->branches[i].first.to_string().c_str(), i*2+1, par->branches[i].second.to_string().c_str());
					update = false;
				}
			}
			else if (stack.back()->is_a<parse_hse::sequence>())
			{
				parse_hse::sequence *par = (parse_hse::sequence*)stack.back();
				if (par->actions.size() == 1)
					stack.back() = par->actions[0];
				else
				{
					for (int i = 0; i < par->actions.size(); i++)
						printf("(%d) %s\n", i, par->actions[i]->to_string().c_str());
					update = false;
				}
			}
			else
				update = false;
		}

		printf("%d %d\n", stack.back()->start, stack.back()->end);

		if (stack.back()->is_a<parse_boolean::disjunction>() || stack.back()->is_a<parse_boolean::internal_choice>())
			printf("before(b) or after(a)?");
		else
			printf("(location)");

		fflush(stdout);
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			fgets(command, 255, script);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if (strncmp(command, "up", 2) == 0 || strncmp(command, "u", 1) == 0)
		{
			stack.pop_back();
			update = true;
		}
		else if (strncmp(command, "done", 4) == 0 || strncmp(command, "d", 1) == 0)
			done = true;
		else if (strncmp(command, "quit", 4) == 0 || strncmp(command, "q", 1) == 0)
			exit(0);
		else if (strncmp(command, "help", 4) == 0 || strncmp(command, "h", 1) == 0)
			print_location_help();
		if (strncmp(command, "before", 6) == 0 || strncmp(command, "b", 1) == 0)
		{
			if (stack.back()->is_a<parse_boolean::disjunction>() || stack.back()->is_a<parse_boolean::internal_choice>())
			{

			}
			else if (sscanf(command, "before %d", &n) == 1 || sscanf(command, "b%d", &n) == 1)
			{
			}
			else
				error("", "before what?", __FILE__, __LINE__);
		}
		else if (strncmp(command, "after", 5) == 0 || strncmp(command, "a", 1) == 0)
		{
			if (stack.back()->is_a<parse_boolean::disjunction>() || stack.back()->is_a<parse_boolean::internal_choice>())
			{

			}
			else if (sscanf(command, "after %d", &n) == 1 || sscanf(command, "a%d", &n) == 1)
			{
			}
			else
				error("", "after what?", __FILE__, __LINE__);
		}
		else if (sscanf(command, "%d", &n) == 1)
		{
			if (stack.back()->is_a<parse_boolean::disjunction>() || stack.back()->is_a<parse_boolean::internal_choice>())
			{

			}
			else
			{
				cout << n << endl;
				if (stack.back()->is_a<parse_hse::parallel>())
				{
					parse_hse::parallel *par = (parse_hse::parallel*)stack.back();
					if (n < par->branches.size())
					{
						stack.push_back(&par->branches[n]);
						update = true;
					}
					else
						error("", "invalid option", __FILE__, __LINE__);
				}
				else if (stack.back()->is_a<parse_hse::condition>())
				{
					parse_hse::condition *par = (parse_hse::condition*)stack.back();
					if (n < par->branches.size()/2)
					{
						stack.push_back(n%2 == 0 ? (parse::syntax*)&par->branches[n/2].first : (parse::syntax*)&par->branches[n/2].second);
						update = true;
					}
					else
						error("", "invalid option", __FILE__, __LINE__);
				}
				else if (stack.back()->is_a<parse_hse::loop>())
				{
					parse_hse::loop *par = (parse_hse::loop*)stack.back();
					if (n < par->branches.size()/2)
					{
						stack.push_back(n%2 == 0 ? (parse::syntax*)&par->branches[n/2].first : (parse::syntax*)&par->branches[n/2].second);
						update = true;
					}
					else
						error("", "invalid option", __FILE__, __LINE__);
				}
				else if (stack.back()->is_a<parse_hse::sequence>())
				{
					parse_hse::sequence *par = (parse_hse::sequence*)stack.back();
					if (n < par->actions.size())
					{
						stack.push_back(par->actions[n]);
						update = true;
					}
					else
						error("", "invalid option", __FILE__, __LINE__);
				}
			}
		}

	}

	return result;
}

void real_time(hse::graph &g, boolean::variable_set &v, string filename)
{
	hse::encoder enc;
	enc.base = &g;

	tokenizer internal_choice_parser(false);
	parse_boolean::internal_choice::register_syntax(internal_choice_parser);

	g.reachability();

	char command[256];
	bool done = false;
	FILE *script = stdin;
	while (!done)
	{
		printf("(hseenc)");
		fflush(stdout);
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			fgets(command, 255, script);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if ((strncmp(command, "help", 4) == 0 && length == 4) || (strncmp(command, "h", 1) == 0 && length == 1))
			print_command_help();
		else if ((strncmp(command, "quit", 4) == 0 && length == 4) || (strncmp(command, "q", 1) == 0 && length == 1))
			done = true;
		else if ((strncmp(command, "elaborate", 9) == 0 && length == 9) || (strncmp(command, "e", 1) == 0 && length == 1))
			g.elaborate(v, true);
		else if ((strncmp(command, "conflicts", 9) == 0 && length == 9) || (strncmp(command, "c", 1) == 0 && length == 1))
		{
			enc.check(true);
			print_conflicts(enc, g, v, -1);
		}
		else if ((strncmp(command, "conflicts up", 12) == 0 && length == 12) || (strncmp(command, "cu", 2) == 0 && length == 2))
		{
			enc.check(false);
			print_conflicts(enc, g, v, 0);
		}
		else if ((strncmp(command, "conflicts down", 14) == 0 && length == 14) || (strncmp(command, "cd", 2) == 0 && length == 2))
		{
			enc.check(false);
			print_conflicts(enc, g, v, 1);
		}
		else if ((strncmp(command, "suspects", 8) == 0 && length == 8) || (strncmp(command, "s", 1) == 0 && length == 1))
		{
			enc.check(true);
			print_suspects(enc, g, v, -1);
		}
		else if ((strncmp(command, "suspects up", 11) == 0 && length == 11) || (strncmp(command, "su", 2) == 0 && length == 2))
		{
			enc.check(false);
			print_suspects(enc, g, v, 0);
		}
		else if ((strncmp(command, "suspects down", 13) == 0 && length == 13) || (strncmp(command, "sd", 2) == 0 && length == 2))
		{
			enc.check(false);
			print_suspects(enc, g, v, 1);
		}
		else if (strncmp(command, "insert", 6) == 0)
		{
			if (length <= 6)
				printf("error: expected expression\n");
			else
			{
				internal_choice_parser.insert("", string(command).substr(6));
				parse_boolean::internal_choice expr(internal_choice_parser);
				boolean::cover action = import_cover(internal_choice_parser, expr, v, true);
				if (internal_choice_parser.is_clean())
				{
					vector<pair<hse::iterator, int> > locations = get_locations(script, g, v);
					for (int i = 0; i < (int)locations.size(); i++)
						printf("%s %c%d\n", locations[i].second ? "after" : "before", locations[i].first.type == hse::place::type ? 'P' : 'T', locations[i].first.index);
				}
			}
		}
		else if (length > 0)
			printf("error: unrecognized command '%s'\n", command);
	}
}

int main(int argc, char **argv)
{
	configuration config;
	config.set_working_directory(argv[0]);
	tokenizer hse_tokens;
	tokenizer dot_tokens;
	parse_hse::parallel::register_syntax(hse_tokens);
	parse_dot::graph::register_syntax(dot_tokens);

	bool c = false, cu = false, cd = false, s = false, su = false, sd = false;

	for (int i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if (arg == "--help" || arg == "-h")			// Help
		{
			print_help();
			return 0;
		}
		else if (arg == "--version")	// Version Information
		{
			print_version();
			return 0;
		}
		else if (arg == "--verbose" || arg == "-v")
			set_verbose(true);
		else if (arg == "--debug" || arg == "-d")
			set_debug(true);
		else if (arg == "-c")
			c = true;
		else if (arg == "-cu")
			cu = true;
		else if (arg == "-cd")
			cd = true;
		else if (arg == "-s")
			s = true;
		else if (arg == "-su")
			su = true;
		else if (arg == "-sd")
			sd = true;
		else
		{
			string filename = argv[i];
			int dot = filename.find_last_of(".");
			string format = "";
			if (dot != string::npos)
				format = filename.substr(dot+1);
			if (format == "hse")
				config.load(hse_tokens, filename, "");
			else if (format == "dot")
				config.load(dot_tokens, filename, "");
			else
				printf("unrecognized file format '%s'\n", format.c_str());
		}
	}

	if (is_clean() && hse_tokens.segments.size() > 0)
	{
		hse::graph g;
		boolean::variable_set v;

		bool first = true;
		hse_tokens.increment(false);
		hse_tokens.expect<parse_hse::parallel>();
		while (hse_tokens.decrement(__FILE__, __LINE__))
		{
			parse_hse::parallel syntax(hse_tokens);
			g.merge(hse::parallel, import_graph(hse_tokens, syntax, v, true), !first);

			hse_tokens.increment(false);
			hse_tokens.expect<parse_hse::parallel>();
			first = false;
		}

		dot_tokens.increment(false);
		dot_tokens.expect<parse_dot::graph>();
		while (dot_tokens.decrement(__FILE__, __LINE__))
		{
			parse_dot::graph syntax(dot_tokens);
			g.merge(hse::parallel, import_graph(dot_tokens, syntax, v, true), !first);

			dot_tokens.increment(false);
			dot_tokens.expect<parse_dot::graph>();
			first = false;
		}
		g.compact(v, true);

		g.reachability();

		g.elaborate(v, false);

		hse::encoder enc;
		enc.base = &g;

		if (c || s)
		{
			enc.check(true);
			if (c)
				print_conflicts(enc, g, v, -1);

			if (s)
				print_suspects(enc, g, v, -1);
		}

		if (cu || cd || su || sd)
		{
			enc.check(false);

			if (cu)
				print_conflicts(enc, g, v, 0);

			if (cd)
				print_conflicts(enc, g, v, 1);

			if (su)
				print_suspects(enc, g, v, 0);

			if (sd)
				print_suspects(enc, g, v, 1);
		}

		if (!c && !cu && !cd && !s && !su && !sd)
			real_time(g, v, "");
	}

	complete();
	return is_clean();
}
