using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_CharOutfit : WOApiWebPage
{
    void OutfitOp(string CustomerID, string CharID, int HeadIdx, int BodyIdx, int LegsIdx)
    {

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_CharChangeOutfit";

        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_HeadIdx", HeadIdx);
        sqcmd.Parameters.AddWithValue("@in_BodyIdx", BodyIdx);
        sqcmd.Parameters.AddWithValue("@in_LegsIdx", LegsIdx);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

        int HeadIdx = web.GetInt("HeadIdx");
        int BodyIdx = web.GetInt("BodyIdx");
        int LegsIdx = web.GetInt("LegsIdx");

        string CustomerID = web.CustomerID();
        string CharID = web.Param("CharID");

        OutfitOp(CustomerID, CharID, HeadIdx, BodyIdx, LegsIdx);
    }
}