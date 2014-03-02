using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_SrvCharUpdate : WOApiWebPage
{
    string CustomerID = null;
    string CharID = null;

    void UpdateCharStatus()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_Char_SRV_SetStatus";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_MapID", web.Param("map"));
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_Alive", web.Param("s1"));
        sqcmd.Parameters.AddWithValue("@in_GamePos", web.Param("s2"));
        sqcmd.Parameters.AddWithValue("@in_Health", web.Param("s3"));
        sqcmd.Parameters.AddWithValue("@in_Hunger", web.Param("s4"));
        sqcmd.Parameters.AddWithValue("@in_Thirst", web.Param("s5"));
        sqcmd.Parameters.AddWithValue("@in_Toxic", web.Param("s6"));
        sqcmd.Parameters.AddWithValue("@in_TimePlayed", web.Param("s7"));
        sqcmd.Parameters.AddWithValue("@in_XP", web.Param("s8"));
        sqcmd.Parameters.AddWithValue("@in_Reputation", web.Param("s9"));
        sqcmd.Parameters.AddWithValue("@in_GameFlags", web.Param("sA"));
        sqcmd.Parameters.AddWithValue("@in_GameDollars", web.Param("sB"));
        // generic trackable stats
        sqcmd.Parameters.AddWithValue("@in_Stat00", web.Param("ts00"));
        sqcmd.Parameters.AddWithValue("@in_Stat01", web.Param("ts01"));
        sqcmd.Parameters.AddWithValue("@in_Stat02", web.Param("ts02"));
        sqcmd.Parameters.AddWithValue("@in_Stat03", web.Param("ts03"));
        sqcmd.Parameters.AddWithValue("@in_Stat04", web.Param("ts04"));
        sqcmd.Parameters.AddWithValue("@in_Stat05", web.Param("ts05"));

        //skills
        sqcmd.Parameters.AddWithValue("@in_SkillID0", web.Param("SkillID0"));
        sqcmd.Parameters.AddWithValue("@in_SkillID1", web.Param("SkillID1"));
        sqcmd.Parameters.AddWithValue("@in_SkillID2", web.Param("SkillID2"));
        sqcmd.Parameters.AddWithValue("@in_SkillID3", web.Param("SkillID3"));
        sqcmd.Parameters.AddWithValue("@in_SkillID4", web.Param("SkillID4"));
        sqcmd.Parameters.AddWithValue("@in_SkillID5", web.Param("SkillID5"));
        sqcmd.Parameters.AddWithValue("@in_SkillID6", web.Param("SkillID6"));
        sqcmd.Parameters.AddWithValue("@in_SkillID7", web.Param("SkillID7"));
        sqcmd.Parameters.AddWithValue("@in_SkillID8", web.Param("SkillID8"));
        sqcmd.Parameters.AddWithValue("@in_SkillID9", web.Param("SkillID9"));
        sqcmd.Parameters.AddWithValue("@in_SkillID10", web.Param("SkillID10"));
        sqcmd.Parameters.AddWithValue("@in_SkillID11", web.Param("SkillID11"));
        sqcmd.Parameters.AddWithValue("@in_SkillID12", web.Param("SkillID12"));
        sqcmd.Parameters.AddWithValue("@in_SkillID13", web.Param("SkillID13"));
        sqcmd.Parameters.AddWithValue("@in_SkillID14", web.Param("SkillID14"));
        sqcmd.Parameters.AddWithValue("@in_SkillID15", web.Param("SkillID15"));
        sqcmd.Parameters.AddWithValue("@in_SkillID16", web.Param("SkillID16"));
        sqcmd.Parameters.AddWithValue("@in_SkillID17", web.Param("SkillID17"));
        sqcmd.Parameters.AddWithValue("@in_SkillID18", web.Param("SkillID18"));
        sqcmd.Parameters.AddWithValue("@in_SkillID19", web.Param("SkillID19"));
        sqcmd.Parameters.AddWithValue("@in_SkillID20", web.Param("SkillID20"));
        sqcmd.Parameters.AddWithValue("@in_SkillID21", web.Param("SkillID21"));
        sqcmd.Parameters.AddWithValue("@in_SkillID22", web.Param("SkillID22"));
        sqcmd.Parameters.AddWithValue("@in_SkillID23", web.Param("SkillID23"));
        sqcmd.Parameters.AddWithValue("@in_SkillID24", web.Param("SkillID24"));
        sqcmd.Parameters.AddWithValue("@in_SkillID25", web.Param("SkillID25"));
        sqcmd.Parameters.AddWithValue("@in_SkillID26", web.Param("SkillID26"));
        sqcmd.Parameters.AddWithValue("@in_SkillID27", web.Param("SkillID27"));
        sqcmd.Parameters.AddWithValue("@in_SkillID28", web.Param("SkillID28"));
        sqcmd.Parameters.AddWithValue("@in_SkillID29", web.Param("SkillID29"));
        sqcmd.Parameters.AddWithValue("@in_SkillID30", web.Param("SkillID30"));
        sqcmd.Parameters.AddWithValue("@in_SkillID31", web.Param("SkillID31"));
        sqcmd.Parameters.AddWithValue("@in_SkillID32", web.Param("SkillID32"));
        sqcmd.Parameters.AddWithValue("@in_SkillID33", web.Param("SkillID33"));
        if (!CallWOApi(sqcmd))
            return;
    }

    void UpdateCharBackpack()
    {
        // use a single transaction
        CloseDataReader();
        using (SqlTransaction transaction = sql.BeginTransaction())
        {
            for (int i = 0; i < 999; i++)
            {
                string BpEntry;
                try
                {
                    BpEntry = web.Param("bp" + i.ToString());
                }
                catch
                {
                    break;
                }

                // c++ sprintf("%d %d %d %d %d %d", slot, isAdding, w1.itemID, w1.quantity, w1.Var1, w1.Var2);
                string[] arr = BpEntry.Split(' ');
                if (arr.Length != 6)
                    throw new ApiExitException("bad BpEntry");

                int Slot = Convert.ToInt32(arr[0]);
                int Op = Convert.ToInt32(arr[1]);
                int ItemID = Convert.ToInt32(arr[2]);
                int Amount = Convert.ToInt32(arr[3]);
                int Var1 = Convert.ToInt32(arr[4]);
                int Var2 = Convert.ToInt32(arr[5]);

                string cmd = "";
                switch (Op)
                {
                    default:
                        throw new ApiExitException("bad op");
                    case 0: // add
                        cmd = "WZ_Backpack_SRV_AddItem";
                        break;
                    case 1: // alter
                        cmd = "WZ_Backpack_SRV_AlterItem";
                        break;
                    case 2: // delete
                        cmd = "WZ_Backpack_SRV_DeleteItem";
                        break;
                }

                SqlCommand sqcmd = new SqlCommand();
                sqcmd.Transaction = transaction;
                sqcmd.CommandType = CommandType.StoredProcedure;
                sqcmd.CommandText = cmd;
                sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
                sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
                sqcmd.Parameters.AddWithValue("@in_Slot", Slot);
                sqcmd.Parameters.AddWithValue("@in_ItemID", ItemID);
                sqcmd.Parameters.AddWithValue("@in_Amount", Amount);
                sqcmd.Parameters.AddWithValue("@in_Var1", Var1);
                sqcmd.Parameters.AddWithValue("@in_Var2", Var2);

                if (!CallWOApi(sqcmd))
                    return;
                CloseDataReader();
            }

            transaction.Commit();
        }
    }

    void UpdateCharAttachments()
    {
        string attm1 = web.Param("attm1");
        string attm2 = web.Param("attm2");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_Char_SRV_SetAttachments";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_Attm1", attm1);
        sqcmd.Parameters.AddWithValue("@in_Attm2", attm2);

        if (!CallWOApi(sqcmd))
            return;
    }

    protected override void Execute()
    {
        // we still need to check login credentials in case of double login from other computer
        if (!WoCheckLoginSession())
            return;

        string skey1 = web.Param("skey1");
        if (skey1 != SERVER_API_KEY)
            throw new ApiExitException("bad key");

        CustomerID = web.CustomerID();
        CharID = web.Param("CharID");

        UpdateCharStatus();
        UpdateCharBackpack();
        UpdateCharAttachments();

        Response.Write("WO_0");
    }
}
