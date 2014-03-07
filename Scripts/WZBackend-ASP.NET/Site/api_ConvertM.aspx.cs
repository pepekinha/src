using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.Configuration;

public partial class api_SrvSkills : WOApiWebPage
{
    void OutfitOp( string Var1,string Var2,string CustomerID)
    {

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_ConvertGD";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_Var1", Var1);
        sqcmd.Parameters.AddWithValue("@in_Var2", Var2);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;
  string CustomerID = web.Param("CustomerID");
       string Var1 = web.Param("Var1");
        string Var2 = web.Param("Var2");

        OutfitOp(Var1,Var2,CustomerID);
    }
}