#include "Parse.h"

#include "ParseImpl.h"
#include "EnumParser.h"
#include "ValueRefParserImpl.h"
#include "ConditionParserImpl.h"

#include "../universe/Species.h"

#include <boost/spirit/include/phoenix.hpp>


#define DEBUG_PARSERS 0

#if DEBUG_PARSERS
namespace std {
    inline ostream& operator<<(ostream& os, const FocusType&) { return os; }
    inline ostream& operator<<(ostream& os, const std::vector<FocusType>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::vector<std::shared_ptr<Effect::EffectsGroup>>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::pair<PlanetType, PlanetEnvironment>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::pair<const PlanetType, PlanetEnvironment>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::map<PlanetType, PlanetEnvironment>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::pair<const std::string, Species*>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::map<std::string, Species*>&) { return os; }
}
#endif

namespace {
    const boost::phoenix::function<parse::detail::is_unique> is_unique_;

    const boost::phoenix::function<parse::detail::insert> insert_;

    struct rules {
        rules() {
            namespace phoenix = boost::phoenix;
            namespace qi = boost::spirit::qi;

            using phoenix::construct;
            using phoenix::insert;
            using phoenix::new_;
            using phoenix::push_back;

            qi::_1_type _1;
            qi::_2_type _2;
            qi::_3_type _3;
            qi::_4_type _4;
            qi::_a_type _a;
            qi::_b_type _b;
            qi::_c_type _c;
            qi::_d_type _d;
            qi::_e_type _e;
            qi::_f_type _f;
            qi::_g_type _g;
            qi::_pass_type _pass;
            qi::_r1_type _r1;
            qi::_val_type _val;
            qi::eps_type eps;

            const parse::lexer& tok = parse::lexer::instance();

            focus_type
                =    tok.Focus_
                >    parse::detail::label(Name_token)        > tok.string [ _a = _1 ]
                >    parse::detail::label(Description_token) > tok.string [ _b = _1 ]
                >    parse::detail::label(Location_token)    > parse::detail::condition_parser [ _c = _1 ]
                >    parse::detail::label(Graphic_token)     > tok.string
                     [ _val = construct<FocusType>(_a, _b, _c, _1) ]
                ;

            foci
                =    parse::detail::label(Foci_token)
                >    (
                            ('[' > +focus_type [ push_back(_r1, _1) ] > ']')
                        |    focus_type [ push_back(_r1, _1) ]
                     )
                ;

            effects
                =    parse::detail::label(EffectsGroups_token) > parse::detail::effects_group_parser() [ _r1 = _1 ]
                ;

            environment_map_element
                =    parse::detail::label(Type_token)        > parse::detail::planet_type_rules().enum_expr [ _a = _1 ]
                >    parse::detail::label(Environment_token) > parse::detail::planet_environment_rules().enum_expr
                     [ _val = construct<std::pair<PlanetType, PlanetEnvironment>>(_a, _1) ]
                ;

            environment_map
                =    ('[' > +environment_map_element [ insert(_val, _1) ] > ']')
                |     environment_map_element [ insert(_val, _1) ]
                ;

            environments
                =    parse::detail::label(Environments_token) > environment_map [ _r1 = _1 ]
                ;

            species_params
                =   ((tok.Playable_ [ _a = true ]) | eps)
                >   ((tok.Native_ [ _b = true ]) | eps)
                >   ((tok.CanProduceShips_ [ _c = true ]) | eps)
                >   ((tok.CanColonize_ [ _d = true ]) | eps)
                    [ _val = construct<SpeciesParams>(_a, _b, _d, _c) ]
                ;

            species_strings
                =    parse::detail::label(Name_token)                   > tok.string
                     [ _pass = is_unique_(_r1, Species_token, _1), _a = _1 ]
                >    parse::detail::label(Description_token)            > tok.string [ _b = _1 ]
                >    parse::detail::label(Gameplay_Description_token)   > tok.string [ _c = _1 ]
                    [ _val = construct<SpeciesStrings>(_a, _b, _c) ]
                ;

            species
                =    tok.Species_
                >    species_strings(_r1) [ _a = _1 ]
                >    species_params [ _b = _1]
                >    parse::detail::tags_parser()(_c)
                >   -foci(_d)
                >   -(parse::detail::label(PreferredFocus_token)        >> tok.string [ _g = _1 ])
                >   -effects(_e)
                >   -environments(_f)
                >    parse::detail::label(Graphic_token) > tok.string
                [ insert_(_r1, phoenix::bind(&SpeciesStrings::name, _a),
                          new_<Species>(_a, _d, _g, _f, _e, _b, _c, _1)) ]
                ;

            start
                =   +species(_r1)
                ;

            focus_type.name("Focus");
            foci.name("Foci");
            effects.name("EffectsGroups");
            environment_map_element.name("Type = <type> Environment = <env>");
            environment_map.name("Environments");
            environments.name("Environments");
            species_params.name("Species Flags");
            species_strings.name("Species Strings");
            species.name("Species");
            start.name("start");

#if DEBUG_PARSERS
            debug(focus_type);
            debug(foci);
            debug(effects);
            debug(environment_map_element);
            debug(environment_map);
            debug(environments);
            debug(species_params);
            debug(species_strings);
            debug(species);
            debug(start);
#endif

            qi::on_error<qi::fail>(start, parse::report_error(_1, _2, _3, _4));
        }

        typedef parse::detail::rule<
            FocusType (),
            boost::spirit::qi::locals<
                std::string,
                std::string,
                Condition::ConditionBase*
            >
        > focus_type_rule;

        typedef parse::detail::rule<
            void (std::vector<FocusType>&)
        > foci_rule;

        typedef parse::detail::rule<
            void (std::vector<std::shared_ptr<Effect::EffectsGroup>>&)
        > effects_rule;

        typedef parse::detail::rule<
            std::pair<PlanetType, PlanetEnvironment> (),
            boost::spirit::qi::locals<PlanetType>
        > environment_map_element_rule;

        typedef parse::detail::rule<
            std::map<PlanetType, PlanetEnvironment> ()
        > environment_map_rule;

        typedef parse::detail::rule<
            void (std::map<PlanetType, PlanetEnvironment>&)
        > environments_rule;

        typedef parse::detail::rule<
            SpeciesParams (),
            boost::spirit::qi::locals<
                bool,
                bool,
                bool,
                bool
            >
        > species_params_rule;

        typedef parse::detail::rule<
            SpeciesStrings (const std::map<std::string, Species*>&),
            boost::spirit::qi::locals<
                std::string,
                std::string,
                std::string
            >
        > species_strings_rule;

        typedef parse::detail::rule<
            void (std::map<std::string, Species*>&),
            boost::spirit::qi::locals<
                SpeciesStrings,
                SpeciesParams,
                std::set<std::string>,  // tags
                std::vector<FocusType>,
                std::vector<std::shared_ptr<Effect::EffectsGroup>>,
                std::map<PlanetType, PlanetEnvironment>,
                std::string             // graphic
            >
        > species_rule;

        typedef parse::detail::rule<
            void (std::map<std::string, Species*>&)
        > start_rule;

        foci_rule                       foci;
        focus_type_rule                 focus_type;
        effects_rule                    effects;
        environment_map_element_rule    environment_map_element;
        environment_map_rule            environment_map;
        environments_rule               environments;
        species_params_rule             species_params;
        species_strings_rule            species_strings;
        species_rule                    species;
        start_rule                      start;
    };
}

namespace parse {
    bool species(std::map<std::string, Species*>& species_) {
        bool result = true;

        for (const boost::filesystem::path& file : ListScripts("scripting/species")) {
            result &= detail::parse_file<rules, std::map<std::string, Species*>>(file, species_);
        }

        return result;
    }
}
