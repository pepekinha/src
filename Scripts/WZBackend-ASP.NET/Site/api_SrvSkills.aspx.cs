using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_SrvSkills : WOApiWebPage
{
    void AddSkill()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_SkillAddNew";
        sqcmd.Parameters.AddWithValue("@in_CharID", web.Param("CharID"));
        sqcmd.Parameters.AddWithValue("@in_SkillID", web.Param("SkillID"));

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        string func = web.Param("func");
        if (func == "add")
            AddSkill();
        else
            throw new ApiExitException("bad func");
    }


}
