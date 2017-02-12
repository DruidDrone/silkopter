#pragma once

#include "res/Resource.h"
#include "video/Pass.h"

namespace q
{
namespace video
{
class Technique : public res::Resource
{
public:
    Technique(Path const& path);
    virtual ~Technique();

    virtual void		unload();
    virtual bool		is_valid() const;

    void				add_pass(Pass const& pass);
    size_t				get_pass_count() const;
    Pass const&			get_pass(size_t idx) const;
    int					find_pass_idx_by_name(std::string const& name) const;

    void				compile();
protected:
private:
    typedef std::map<std::string, size_t> NameIdxMap;

    std::vector<Pass>	m_passes;
    NameIdxMap			m_pass_map;
};

}
}
