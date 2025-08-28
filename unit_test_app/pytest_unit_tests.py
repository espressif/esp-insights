from pytest_embedded_idf.dut import IdfDut

def test_data_store(dut: IdfDut):
    dut.run_all_single_board_cases(group="data-store")
